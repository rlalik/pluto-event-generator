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
#include <map>
#include <vector>

#include <getopt.h>

PParticle * p_b = nullptr;  //Beam proton
PParticle * p_t = nullptr;     //Target proton
PParticle * q = nullptr;

//#define NSTARS 1        // define custom Nstar resonances
#define LOOP_DEF 1

/// This will hold the reaction graph after.
/// Owns the children nodes.
struct reaction_node {
    int pid{0};
    std::string name;
    reaction_node * mother{nullptr};
    std::vector<std::unique_ptr<reaction_node>> daughters;
};

/// Add daughter node.
/// @param mother mother node
/// @param pid the children pid
/// @param name the children name
/// @return added children reaction node
auto add_daughter(reaction_node * mother, int pid, std::string name) -> reaction_node * {
    auto d = std::unique_ptr<reaction_node>(new reaction_node());
    d->pid = pid;
    d->name = std::move(name);
    d->mother = mother;
    auto ptr = d.get();
    mother->daughters.push_back(std::move(d));
    return ptr;
}

/// Print the one-liner raction scheme, does not end line
/// @param mother the node from which start printing
auto print_reactions(reaction_node * mother) -> void {
    int counter = 0;
    for (const auto & daughter : mother->daughters) {
        if (counter++ > 0) { printf(" + "); }
        printf("%s", daughter->name.c_str());
        if (daughter->daughters.size() > 0) {
            printf(" [ ");
            print_reactions(daughter.get());
            printf(" ]");
        }
    }
}

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

/// Generate reaction_node tree from the reaction string
/// @param line the reaction string
/// @param mother the top-level node, should be default-initialized node without pid ::mother member initialized
/// @param current_stop the parsing position, usually should be 0
/// @param level the recursion level, should be 0
/// @param verbose print parsing steps
auto parse_reaction(const std::string line, reaction_node * mother, size_t & current_stop, int level = 0, bool verbose = false) -> void
{
    if (verbose) printf("\n");

    while (current_stop != string::npos)
    {
        if (verbose) printf("[%d] Parsing string: '%s'\n", level, line.c_str() + current_stop);

        // If decay_range closing marker, return from the leve
        if (check_decay_stop(line, current_stop))
        {
            if (level == 0) { abort(); }
            current_stop++;
            if (verbose) printf("End of parsing level %d\n", level);
            break;
        }

        auto next_stop = line.find_first_of(",[]", current_stop);

        // Get the particle
        reaction_node * particle_node = nullptr;
        std::string particle_name = line.substr(current_stop, next_stop-current_stop);
        if (verbose) printf("[%d] *** Particle name is '%s', current stop: %lu  next stop: %lu\n\n", level, particle_name.c_str(), current_stop, next_stop);

        // Check if this is decay command, abort if decay at level 0 -- forbidden!
        // If not decay, add it to he list
        if (strncmp(particle_name.c_str(), "decay", 5) == 0) {
            if (level == 0) { abort(); }

            if (verbose) printf("[%d] Entering decay mode\n", level);
            auto decay_stop = line.find_first_of(",[]", current_stop);
            auto decay_string = line.substr(current_stop, decay_stop - current_stop);
            add_daughter(mother, -1, "decay");
            current_stop = next_stop + 1;
            return;
        }
        else
        {
            auto pid = makeStaticData()->GetParticleID(particle_name.c_str());
            particle_node = add_daughter(mother, pid, std::move(particle_name));
        }

        // If the particle is followed by the decay marker, do recursive decay parsing
        // This is executed when the decay marker '[' is found
        if (check_decay_start(line, next_stop))
        {
            current_stop = next_stop+1;
            parse_reaction(line, particle_node, current_stop, level+1);

            if (verbose) printf("[%d] Back at level -- current stop: %lu next stop: %lu\n", level, current_stop, next_stop);
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
}

/// Convert reaction_node tree into PLuto's decay channels
/// @param mother top-level node
/// @param pdm the decay manager
auto compile_reactions(reaction_node * mother, PDecayManager *pdm, int level = 0) -> PDecayChannel *
{
    PDecayChannel * decay_channels = new PDecayChannel();
    const char ** decay_channel_ids = new const char*[mother->daughters.size()];

    int idx = 0;
    for (const auto & part : mother->daughters) {
        if (part->pid == -1) {
            build_recursive_decay(part->mother->name.c_str(), part->name.c_str()+5, pdm);
        }
        else
        {
            decay_channel_ids[idx++] = part->name.c_str();

            if (part->daughters.size() > 0) {
                compile_reactions(part.get(), pdm, level + 1);
            }
        }
        // printf("%*c   part id:  %4d  %s  %d\n", level+1, ' ', part->pid, part->name.c_str());
    }

    if (idx) {
        decay_channels->AddChannel(1.0, idx, decay_channel_ids);

        if (mother->mother) {
            // printf("%*c   -> BR: ", level+1, ' ');
            decay_channels->Print();printf("\n");
            pdm->AddChannel(mother->name.c_str(), decay_channels);
        }
    }
    delete [] decay_channel_ids;

    return decay_channels;
}

/// The database channel record
struct channel_data {
    int id;                 ///< channel id
    float width;            ///< decay width
    float crosssection;     ///< channel cross-section
    std::string body;  ///< the reaction body
};

auto load_database(const char * dbfile) -> std::map<int, channel_data> {
    std::map<int, channel_data> channels;

    std::ifstream ofs(dbfile, std::ios::in);

    if (ofs.is_open()) {
        std::string line;
        while (std::getline(ofs, line)) {
            trim(line);
            if (line.length() ==0) { continue; }
            if (line[0] == '@') { continue; }

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
            if (channels.find(channel) != channels.end()) {
                fprintf(stderr, "Channel %d already exists in the database\n", channel);
                exit(EXIT_FAILURE);
            }
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
        if (!q) {
            p_b = new PParticle("p", beam_energy);  //Beam proton
            p_t = new PParticle("p");     //Target proton
            q = new PParticle(*p_b + *p_t);
        }

        PDecayManager *pdm = new PDecayManager;

        size_t sta = 0;
        reaction_node reaction_mother;

        parse_reaction(channel_string, &reaction_mother, sta);
        auto decay_channels = compile_reactions(&reaction_mother, pdm);

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

/// Print the basic one-liner info about the channel:
///  - channel id
///  - sqrt(s) of the beam+target
///  - total mass of the channel
///  - uses ! to mark channels where total mass exceeds available energy
///  - the reaction body
/// @param channel_id channel id
/// @param channel_string channel body
/// @param energy the beam energy
/// @return false if the total mass exceeds availabe energy, otherwise true
auto query_channel(int channel_id, std::string channel_string, float beam_energy) -> bool {
    PStrangenessPlugin::EnableExperimentalDecays(true);
    makeDistributionManager()->Exec("strangeness:init");

    if (!q) {
        p_b = new PParticle("p", beam_energy);  //Beam proton
        p_t = new PParticle("p");     //Target proton
        q = new PParticle(*p_b + *p_t);
    }

    size_t sta = 0;
    reaction_node reaction_mother;

    parse_reaction(channel_string, &reaction_mother, sta);

    // q->Print();

    float total_mass = 0.;
    for (const auto & part : reaction_mother.daughters) {
        auto mass = makeStaticData()->GetParticleMass(part->pid);
        // printf("part %d  %s  m=%f MeV/c2\n", part->pid, part->name.c_str(), mass);
        total_mass += mass;
    }

    printf("Channel %4d :", channel_id);
    printf("  sqrt(s) = %f  (T=%g)", q->M(), beam_energy);
    printf("  M = %f %c  ", total_mass, total_mass > q->M() ? '!' : ' ');
    print_reactions(&reaction_mother);
    putchar('\n');

    return q->M() > total_mass;
}

int main(int argc, char **argv) {
    int verbose_flag = 0;
    int par_random_seed = 0;
    int par_events = 10000;
    int par_loops = 1;
    int par_scale = 0;
    int par_seed = 0;
    int par_query = 0;
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
        {"query",        no_argument,        &par_query,       1},
        {"database",     required_argument,  0,                'd'},
        {"events",       required_argument,  0,                'e'},
        {"energy",       required_argument,  0,                'E'},
        {"help",         no_argument,        0,                'h'},
        {"loops",        required_argument,  0,                'l'},
        {"output",       required_argument,  0,                'o'},
        {"seed",         required_argument,  0,                's'},
        {"scale",        required_argument,  0,                'x'},
        { 0, 0, 0, 0 }
    };

    Int_t c = 0;
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "d:e:E:hl:o:s:x:", long_options, &option_index);
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

    if (optind == argc) {
        if (par_query) {
            auto channels = load_database(par_database.c_str());
            for (const auto & channel : channels)
                query_channel(channel.first, channel.second.body, Eb);
            exit(EXIT_SUCCESS);
        }

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

        if (par_query) {
            if (!query_channel(selected_channel, chit->second.body, Eb))
                exit(EXIT_FAILURE);
        }
        else
        {
            run_channel(selected_channel, chit->second.body, par_events, par_seed, par_loops, Eb, par_output);
        }
    }

    exit(EXIT_SUCCESS);
}
