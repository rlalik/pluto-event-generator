#include <PDecayChannel.h>
#include <PDecayManager.h>
#include <PParticle.h>
#include <plugins/PStrangenessPlugin.h>

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <getopt.h>

//#define NSTARS 1        // define custom Nstar resonances
#define LOOP_DEF 1


std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    return split(s, delim, elems);
}

// trim from start
static inline std::string &ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}

static inline std::string & replace_chars(std::string &str, char s, char c)
{
    size_t pos = 0;
    while (true)
    {
        pos = str.find(s, pos);
        if (pos == str.npos)
            break;

        str.replace(pos, 1, 1, c);
    }
    return str;
}

static inline std::string & clear_chars(std::string &str, char s)
{
    size_t pos = 0;
    while (true)
    {
        pos = str.find(s, pos);
        if (pos == str.npos)
            break;

        str.erase(pos, 1);
    }
    return str;
}

constexpr auto max_decays = 7;
constexpr const char * decay_product[max_decays] = { "d1", "d2", "d3", "d4", "d5", "d6", "d7" };

/// Get particle and generate list of all decay channels. If the decay channel contains value from allowed_decays,
/// then go recursively there.
/// @param mother_name mother paticle name, must be the name not alias TODO
/// @param allowed_decays string of allowed decays to be recursively parsed,
///        must be in form of list of ':' surrounded names, e.g.g ":part1:part2:...:"
/// @param pdm decay manager to add the decay channels
auto build_recursive_decay(const char * mother_name, const TString & allowed_decays, PDecayManager *pdm) -> void
{
    auto mother_key = makeStaticData()->GetParticleKey(mother_name);

    PDecayChannel * decay_channels = new PDecayChannel();

    int decay_key = -1;
    while (makeDataBase()->MakeListIterator(mother_key, "pnmodes", "link", &decay_key)) {
        auto decay_idx = makeStaticData()->GetDecayIdxByKey(decay_key);
        auto decay_nparts = makeStaticData()->GetDecayNProducts(decay_idx);

        const char *result_name = 0;
        if(!makeDataBase()->GetParamString(decay_key, makeDataBase()->GetParamString("name"), &result_name)) { abort(); }

        const char ** decay_channel_ids = new const char*[decay_nparts];
        if (!decay_nparts) { return; }

        for (int i = 0; i < decay_nparts; ++i) {
            int * decay_particle;
            const char *result_name = 0;
            // const char *result_alias = 0; TODO

            if (!makeDataBase()->GetParamInt(decay_key, makeDataBase()->GetParamInt(decay_product[i]), &decay_particle)) { abort(); }
            if (!makeDataBase()->GetParamString(*decay_particle, makeDataBase()->GetParamString("name"), &result_name)) { abort(); }
            // if (!makeDataBase()->GetParamString(*decay_particle, makeDataBase()->GetParamString("lalias"), &result_alias)) { }

            decay_channel_ids[i] = result_name;

            TString children = TString::Format(":%s:", result_name);
            if (allowed_decays.Contains(children)) { build_recursive_decay(result_name, allowed_decays, pdm); }
        }

        double * result_br = nullptr;
        if (!makeDataBase()->GetParamDouble(decay_key, makeDataBase()->GetParamDouble("br"), &result_br)) { abort(); }
        decay_channels->AddChannel(*result_br, decay_nparts, decay_channel_ids);
    }

    pdm->AddChannel(mother_name, decay_channels);
}

/// Check if the string contains decay marker on the next position
bool check_decay_start(const std::string line, size_t position)
{
    if (position != string::npos and line[position] == '[')
        return true;

    return false;
}

bool check_decay_stop(const std::string line, size_t position)
{
    if (position != string::npos and line[position] == ']')
        return true;

    return false;
}

bool check_separator(const std::string line, size_t position)
{
    if (position != string::npos and line[position] == ',')
        return true;

    return false;
}

auto parse_reaction(const std::string line, const std::string & mother, size_t & current_stop, PDecayManager *pdm,  int level = 0) -> PDecayChannel*
{
    printf("\n");
    // printf("Line to parse: %s starting from %lu\n               ", line.c_str(), current_stop);
    // for (int i = 0; i < line.length(); ++i) { if (i % 5 == 0) printf("|"); else printf("."); } printf("\n");
    std::vector<std::string> particles_list;

    while (current_stop != string::npos)
    {
        printf("[%d] Parsing string: '%s'\n", level, line.c_str() + current_stop);

        // If decay_range closing marker, return from the leve
        if (check_decay_stop(line, current_stop))
        {
            if (level == 0) { abort(); }
            current_stop++;
            printf("End of parsing level %d\n", level);
            break;
        }

        auto next_stop = line.find_first_of(",[]", current_stop);

        // Get the particle
        std::string particle_name = line.substr(current_stop, next_stop-current_stop);
        printf("[%d] *** Particle name is '%s', current stop: %lu  next stop: %lu\n\n", level, particle_name.c_str(), current_stop, next_stop);

        // Check if this is decay command, abort if decay at level 0 -- forbidden!
        // If not decay, add it to he list
        if (strncmp(particle_name.c_str(), "decay", 5) == 0) {
            if (level == 0) { abort(); }

            printf("[%d] Entering decay mode\n", level);
            auto decay_stop = line.find_first_of(",[]", current_stop);
            auto decay_string = line.substr(current_stop, decay_stop - current_stop);
            build_recursive_decay(mother.c_str(), decay_string.c_str()+5, pdm);
            current_stop = next_stop + 1;
            return nullptr;
        }
        else
        {
            particles_list.push_back(particle_name);
        }

        // If the particle is followed by the decay marker, do recursive decay parsing
        // This is executed when the decay marker '[' is found
        if (check_decay_start(line, next_stop))
        {
            current_stop = next_stop+1;
            parse_reaction(line, particle_name,  current_stop, pdm, level+1);

            printf("[%d] Back at level -- current stop: %lu next stop: %lu\n", level, current_stop, next_stop);
            if (line[current_stop] == ',') { current_stop++; }
            next_stop = current_stop;
        }

        if (check_separator(line, next_stop))
        {
            current_stop = next_stop + 1;
        }
        else
        {
            current_stop = next_stop;
        }

        if (next_stop >= line.length()) { current_stop = string::npos; }
    }

    printf("[%d] FILL DECAY CHANNELS\n", level);
    PDecayChannel * decay_channels = new PDecayChannel();
    const char ** decay_channel_ids = new const char*[particles_list.size()];
    int idx = 0;
    for (const auto & part : particles_list) {
        decay_channel_ids[idx++] = part.c_str();
        printf("   part = %s\n", part.c_str());
    }
    decay_channels->AddChannel(1.0, idx, decay_channel_ids);

    if (mother.length() > 0) {
        pdm->AddChannel(mother.c_str(), decay_channels);
    }

    return decay_channels;
}

struct channel_data {
    int id;
    float width;
    float crosssection;
    std::string particles;
};



auto load_database(const char * dbfile) -> std::unordered_map<int, channel_data> {
    std::unordered_map<int, channel_data> channels;

    std::ifstream ofs(dbfile, std::ios::in);

    if (ofs.is_open()) {
        std::string line;
        while (std::getline(ofs, line)) {
            if (line[0] == '@')
                continue;

            int channel;
            float width;
            float crosssection;
            std::string particles;
            std::string comment;

            std::istringstream iss(line);
            iss >> channel >> width >> crosssection >> particles;
            std::getline(iss, comment);
            trim(comment);

            std::string _parts = particles;
            replace_chars(_parts, '@', ',');
            clear_chars(_parts, '[');
            clear_chars(_parts, ']');

            channel_data chd = { channel, width, crosssection, particles };
            channels[channel] = std::move(chd);
        }
    }
    else
    {
        std::cerr << "Database " << dbfile << " can not be open! Exiting..." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    return channels;
}

auto init_nstar() -> void
{
    #ifdef NSTARS
    #ifdef LOOP_DEF
    const int nstar_cnt = 8;

    int nstar_num[nstar_cnt] = { 1650, 1710, 1720, 1875, 1880, 1895, 1900, 2190 };
    float nstar_mass[nstar_cnt] = { 1.655, 1.710, 1.720, 1.875, 1.870, 1.895, 1.900, 2.190 };
    float nstar_width[nstar_cnt] = { 0.150, 0.200, 0.250, 0.220, 0.235, 0.090, 0.250, 0.0250 };

    for (int i = 0; i < nstar_cnt; ++i)
    {
        char * res_decay = new char[200];
        char * res_name = new char[200];

        sprintf(res_name, "NStar%d", nstar_num[i]);
        sprintf(res_decay, "%s --> Lambda + K+", res_name);
        printf("--- [%d] Adding resonance id = %d: %s : %s\n", i, nstar_num[i], res_name, res_decay);
        pid_resonance = makeStaticData()->AddParticle(-1, res_name, nstar_mass[i]);  //Mass in GeV/c2
        printf("   PID resonance: %d\n", pid_resonance);
        makeStaticData()->SetParticleTotalWidth(res_name, nstar_width[i]);
        makeStaticData()->SetParticleBaryon(res_name, 1);
        decay_index = makeStaticData()->AddDecay(res_decay, res_name, "Lambda, K+", 1);
        listParticle(res_name);
    }
    #else

    {
        pid_resonance = makeStaticData()->AddParticle(-1,"NStar1650",1.650);  //Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar1650",0.100);
        makeStaticData()->SetParticleBaryon("NStar1650",1);
        decay_index = makeStaticData()->AddDecay(" NStar1650--> Lambda + K+", "NStar1650","Lambda, K+", 1);
        listParticle("NStar1650");
    }

    {
        pid_resonance = makeStaticData()->AddParticle(-1,"NStar1710",1.710); //Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar1710",0.2);
        makeStaticData()->SetParticleBaryon("NStar1710",1);
        decay_index = makeStaticData()->AddDecay("NStar1710 --> Lambda + K+", "NStar1710","Lambda, K+", 1);
        listParticle("NStar1710");
    }

    {
        pid_resonance = makeStaticData()->AddParticle(-1,"NStar1720",2.000); //Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar1720",0.2);
        makeStaticData()->SetParticleBaryon("NStar1720",1);
        decay_index = makeStaticData()->AddDecay("NStar1720 --> Lambda + K+", "NStar1720","Lambda, K+", 1);
        listParticle("NStar1720");
    }

    {
        pid_resonance = makeStaticData()->AddParticle(-1,"NStar1900",2.400); //Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar1900",0.200);
        makeStaticData()->SetParticleBaryon("NStar1900",1);
        decay_index = makeStaticData()->AddDecay("NStar1900 --> Lambda + K+", "NStar1900","Lambda, K+", 1);
        listParticle("NStar1900");
    }

    {
        pid_resonance = makeStaticData()->AddParticle(-1,"NStar2190",2.80); //Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar2190",0.200);
        makeStaticData()->SetParticleBaryon("NStar2190",1);
        decay_index = makeStaticData()->AddDecay("NStar2190 --> Lambda + K+", "NStar2190","Lambda, K+", 1);
        listParticle("NStar2190");
    }

    #endif

    #endif /* NSTARS */
}

auto run_channel(int channel_id, std::string channel_string, int events, int seed, int loops, float beam_energy, std::string output_dir) -> void
{
    // if (!has_data) {
    //     std::cerr << "No data for given channel " << selected_channel << "! Exiting..." << std::endl;
    //     std::exit(EXIT_FAILURE);
    // }

    TString detect_dalitz = channel_string;
    if (detect_dalitz.Contains("dilepton")) {
        PStrangenessPlugin::EnableHadronDecays(false);
        PStrangenessPlugin::EnablePhotonDecays(false);
    } else {
        PStrangenessPlugin::EnableDalitzDecays(false);
    }

    makeDistributionManager()->Exec("strangeness:init");

    auto list_strange = false;
    if (list_strange) {
        listParticle("Lambda");
        listParticle("Sigma0");
        listParticle("Sigma+");
        listParticle("Sigma-");
        listParticle("Xi0");
        listParticle("Xi-");
        listParticle("Sigma13850");
        listParticle("Sigma1385+");
        listParticle("Sigma1385-");
        listParticle("Lambda1405");
        listParticle("Lambda1520");
        listParticle("Xi15300");
        listParticle("Xi1530-");
    }

    printf("******************* selected channel: %d ***************************\n", channel_id);

    for (int l = seed; l < seed+loops; ++l)
    {
        PUtils::SetSeed(l);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        char buff[200];
        if (output_dir.length())
            sprintf(buff, "%s/pluto_chan_%03d_events_%d_seed_%04d", output_dir.c_str(), channel_id, events, l);
        else
            sprintf(buff, "pluto_chan_%03d_events_%d_seed_%04d", channel_id, events, l);

        std::string tmpname = buff;

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        //Some Particles for the reaction

#ifndef NSTARS
        PParticle * p_b = new PParticle("p", beam_energy);  //Beam proton
        PParticle * p_t = new PParticle("p");     //Target proton
        PParticle * q = new PParticle(*p_b + *p_t);

        PDecayManager *pdm = new PDecayManager;

        size_t sta = 0;
        auto decay_channels = parse_reaction(channel_string, "", sta, pdm);

        pdm->InitReaction(q, decay_channels);
        pdm->Print();

        /*Int_t n = */pdm->loop(events, 0, const_cast<char *>(tmpname.c_str()), 0, 0, 0, 1, 1);
#else

        char str_en[20];
        sprintf(str_en, "%f", Eb);
        std::string _part = particles;
        replace_chars(_part, '@', ' ');
        replace_chars(_part, ',', ' ');

        PReaction my_reaction(str_en, "p", "p", const_cast<char *>(_part.c_str()), const_cast<char *>(tmpname.c_str()), 0, 0, 0, 1);
        my_reaction.Print();
        my_reaction.loop(par_events);
#endif
    }

    printf("Generator finished its job, good bye!\n");
}

int main(int argc, char **argv) {
    int verbose_flag = 0;
    int par_random_seed = 0;
    int par_events = 10000;
    int par_loops = 1;
    int par_scale = 0;
    int par_seed = 0;
    int par_query = -1;
    int list_strange = 0;
    Float_t Eb = 4.5;         // beam energy in AGeV

    std::string par_database = "ChannelsDatabase.txt";
    std::string par_output = "";

    struct option long_options[] =
    {
        {"verbose",      no_argument,        &verbose_flag,    1},
        {"brief",        no_argument,        &verbose_flag,    0},
        {"random-seed",  no_argument,        &par_random_seed, 1},
        {"list-strange", no_argument,        &list_strange,    1},
        {"database",     required_argument,  0,                'd'},
        {"events",       required_argument,  0,                'e'},
        {"energy",       required_argument,  0,                'E'},
        {"help",         no_argument,        0,                'h'},
        {"loops",        required_argument,  0,                'l'},
        {"output",       required_argument,  0,                'o'},
        {"query",        required_argument,  0,                'q'},
        {"seed",         required_argument,  0,                's'},
        {"scale",        required_argument,  0,                'x'},
        { 0, 0, 0, 0 }
    };

    Int_t c = 0;
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "d:e:E:hl:o:q:s:x:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 0:
                if (long_options[option_index].flag != 0)
                    break;
            printf ("option %s", long_options[option_index].name);
            if (optarg)
                printf (" with arg %s", optarg);
            printf ("\n");
            break;
            case 'd':
                par_database = optarg;
                break;
            case 'e':
                par_events = atoi(optarg);
                break;
            case 'E':
                Eb = atof(optarg);
                break;
            case 'h':
                //                 Usage();
                exit(EXIT_SUCCESS);
                break;
            case 'l':
                par_loops = atoi(optarg);
                break;
            case 'o':
                par_output = optarg;
                break;
            case 'q':
                par_query = atoi(optarg);
                break;
            case 's':
                par_seed = atoi(optarg);
                break;
            case 'x':
                par_scale = atoi(optarg);
                break;
            case '?':
                //                 abort();
                break;
            default:
                break;
        }
    }

    if (par_query >= 0) {
        auto channels = load_database(par_database.c_str());

        const auto chit = channels.find(par_query);
        if (chit != channels.end()) {
            printf("%s\n", chit->second.particles.c_str());
            exit(EXIT_SUCCESS);
        }

        exit(EXIT_FAILURE);
    }

    if (optind == argc) {
        std::cerr << "No channel given! Exiting..." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    auto channels = load_database(par_database.c_str());

    while (optind < argc) {
        auto selected_channel = atoi(argv[optind++]);
        const auto chit = channels.find(selected_channel);
        if (chit == channels.end()) {
            printf("Channel %d is missing\n", selected_channel);
            exit(EXIT_FAILURE);
        }

        run_channel(selected_channel, chit->second.particles, par_events, par_seed, par_loops, Eb, par_output);
    }

    exit(EXIT_SUCCESS);
}
