#include <PDecayChannel.h>
#include <PDecayManager.h>
#include <PParticle.h>
#include <plugins/PMesonsPlugin.h>
#include <plugins/PStrangenessPlugin.h>
#include <PResonanceDalitz.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <getopt.h>

namespace
{
PParticle* p_b = nullptr; // Beam particle
PParticle* p_t = nullptr; // Target particle
PParticle* q = nullptr;
float beam_energy = 0.0;
int verbose_flag = 0;
} // namespace

// #define NSTARS 1        // define custom Nstar resonances
#define LOOP_DEF 1

// *********************************************************************************************************************

/// This will hold the reaction graph after.
/// Owns the children nodes.
struct reaction_node
{
    int pid{0};
    std::string name;
    reaction_node* mother{nullptr};
    std::vector<std::unique_ptr<reaction_node>> daughters;
};

/// Add daughter node.
/// @param mother mother node
/// @param pid the children pid
/// @param name the children name
/// @return added children reaction node
auto add_daughter(reaction_node* mother, int pid, std::string name) -> reaction_node*
{
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
auto print_reactions(reaction_node* mother) -> void
{
    int counter = 0;
    for (const auto& daughter : mother->daughters)
    {
        if (counter++ > 0) { printf(" + "); }
        printf("%s", daughter->name.c_str());
        if (daughter->daughters.size() > 0)
        {
            printf(" [ ");
            print_reactions(daughter.get());
            printf(" ]");
        }
    }
}

// *********************************************************************************************************************

// String manipulation functions

std::vector<std::string>& split(const std::string& s, char delim, std::vector<std::string>& elems)
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
    {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string& s, char delim)
{
    std::vector<std::string> elems;
    return split(s, delim, elems);
}

// trim from start
static inline std::string& ltrim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
static inline std::string& rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline std::string& trim(std::string& s) { return ltrim(rtrim(s)); }

static inline std::string& replace_chars(std::string& str, char s, char c)
{
    size_t pos = 0;
    while (true)
    {
        pos = str.find(s, pos);
        if (pos == str.npos) break;

        str.replace(pos, 1, 1, c);
    }
    return str;
}

static inline std::string& clear_chars(std::string& str, char s)
{
    size_t pos = 0;
    while (true)
    {
        pos = str.find(s, pos);
        if (pos == str.npos) break;

        str.erase(pos, 1);
    }
    return str;
}

// *********************************************************************************************************************

/// Configure the collision system.
/// If !b and !c then beam energy will be only set. One energy defined, cannot be overriden later.
/// @param b beam particle
/// @param t target particle
/// @param energy beam particle energy
/// @return system was initialized
auto init_collision_system(const char* b, const char* t, float energy) -> bool
{
    if (!q and !b and !t)
    {
        beam_energy = energy;
        return false;
    }

    if (!q)
    {
        if (beam_energy == 0.0) { beam_energy = energy; }

        p_b = new PParticle(b, beam_energy); // Beam proton
        p_t = new PParticle(t);              // Target proton
        q = new PParticle(*p_b + *p_t);
        return true;
    }

    return false;
}

auto init_collision_system(std::string collision_system) -> bool
{
    auto system_parts = split(trim(collision_system), ',');

    if (system_parts.size() != 3)
    {
        fprintf(stderr, "Incorrect collision system string: %s\n", collision_system.c_str());
        abort();
    }

    float energy = 0.;
    try
    {
        energy = stof(system_parts[2].c_str());
    }
    catch (const std::invalid_argument&)
    {
        fprintf(stderr, "Invalid beam energy format: %s, must be of float type.\n", system_parts[2].c_str());
        abort();
    }

    if (!system_parts[0].length() or !system_parts[1].length() or energy == 0.)
    {
        fprintf(stderr, "Ill-formed collision system string: %s -> %s %s %s\n", collision_system.c_str(), system_parts[0].c_str(),
                system_parts[1].c_str(), system_parts[2].c_str());
        abort();
    }

    auto r = init_collision_system(system_parts[0].c_str(), system_parts[1].c_str(), energy);
    // printf("Collision system %s init: %d\n", collision_system.c_str(), r);
    return r;
}

auto check_collision_system() -> void
{
    if (!q)
    {
        fprintf(stderr, "Collision system not initialized.\n");
        abort();
    }
}

// *********************************************************************************************************************

constexpr auto max_decays = 7;
constexpr const char* decay_product[max_decays] = {"d1", "d2", "d3", "d4", "d5", "d6", "d7"};

/// Get particle and generate list of all decay channels. If the decay channel contains value from allowed_decays,
/// then go recursively there.
/// @param mother_name mother paticle name, must be the name not alias TODO
/// @param allowed_decays string of allowed decays to be recursively parsed,
///        must be in form of list of ':' surrounded names, e.g.g ":part1:part2:...:"
/// @param pdm decay manager to add the decay channels
auto build_recursive_decay(const char* mother_name, const TString& allowed_decays, PDecayManager* pdm) -> void
{
    auto mother_key = makeStaticData()->GetParticleKey(mother_name);

    PDecayChannel* decay_channels = new PDecayChannel();

    int decay_key = -1;
    while (makeDataBase()->MakeListIterator(mother_key, "pnmodes", "link", &decay_key))
    {
        auto decay_idx = makeStaticData()->GetDecayIdxByKey(decay_key);
        auto decay_nparts = makeStaticData()->GetDecayNProducts(decay_idx);

        const char* result_name = 0;
        if (!makeDataBase()->GetParamString(decay_key, makeDataBase()->GetParamString("name"), &result_name)) { abort(); }

        const char** decay_channel_ids = new const char*[decay_nparts];
        if (!decay_nparts) { return; }

        for (int i = 0; i < decay_nparts; ++i)
        {
            int* decay_particle;
            const char* result_name = 0;
            // const char *result_alias = 0; TODO

            if (!makeDataBase()->GetParamInt(decay_key, makeDataBase()->GetParamInt(decay_product[i]), &decay_particle)) { abort(); }
            if (!makeDataBase()->GetParamString(*decay_particle, makeDataBase()->GetParamString("name"), &result_name)) { abort(); }
            // if (!makeDataBase()->GetParamString(*decay_particle, makeDataBase()->GetParamString("lalias"),
            // &result_alias)) { }

            decay_channel_ids[i] = result_name;

            TString children = TString::Format(":%s:", result_name);
            if (allowed_decays.Contains(children)) { build_recursive_decay(result_name, allowed_decays, pdm); }
        }

        double* result_br = nullptr;
        if (!makeDataBase()->GetParamDouble(decay_key, makeDataBase()->GetParamDouble("br"), &result_br)) { abort(); }
        decay_channels->AddChannel(*result_br, decay_nparts, decay_channel_ids);
    }

    pdm->AddChannel(mother_name, decay_channels);
}

/// Check if the string contains decay marker on the next position
bool check_decay_start(const std::string line, size_t position)
{
    if (position != string::npos and line[position] == '[') return true;

    return false;
}

bool check_decay_stop(const std::string line, size_t position)
{
    if (position != string::npos and line[position] == ']') return true;

    return false;
}

bool check_separator(const std::string line, size_t position)
{
    if (position != string::npos and line[position] == ',') return true;

    return false;
}

/// Generate reaction_node tree from the reaction string
/// @param line the reaction string
/// @param mother the top-level node, should be default-initialized node without pid ::mother member initialized
/// @param current_stop the parsing position, usually should be 0
/// @param level the recursion level, should be 0
/// @param verbose print parsing steps
auto parse_reaction(const std::string line, reaction_node* mother, size_t& current_stop, int level = 0, bool verbose = false) -> void
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
        reaction_node* particle_node = nullptr;
        std::string particle_name = line.substr(current_stop, next_stop - current_stop);
        if (verbose)
            printf("[%d] *** Particle name is '%s', current stop: %lu  next stop: %lu\n\n", level, particle_name.c_str(), current_stop,
                   next_stop);

        // Check if this is decay command, abort if decay at level 0 -- forbidden!
        // If not decay, add it to he list
        if (strncmp(particle_name.c_str(), "decay", 5) == 0)
        {
            if (level == 0) { abort(); }

            if (verbose) printf("[%d] Entering decay mode for mother=%s\n", level, mother->name.c_str());
            auto decay_stop = line.find_first_of(",[]", current_stop);
            auto decay_string = line.substr(current_stop, decay_stop - current_stop);
            add_daughter(mother, -1, particle_name.c_str());
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
            current_stop = next_stop + 1;
            parse_reaction(line, particle_node, current_stop, level + 1, verbose);

            if (verbose) printf("[%d] Back at level -- current stop: %lu next stop: %lu\n", level, current_stop, next_stop);
            if (line[current_stop] == ',') { current_stop++; }
            next_stop = current_stop;
        }

        if (check_separator(line, next_stop)) { current_stop = next_stop + 1; }
        else { current_stop = next_stop; }

        if (next_stop >= line.length()) { current_stop = string::npos; }
    }
}

/// Convert reaction_node tree into PLuto's decay channels
/// @param mother top-level node
/// @param pdm the decay manager
auto compile_reactions(reaction_node* mother, PDecayManager* pdm, int level = 0) -> PDecayChannel*
{
    PDecayChannel* decay_channels = new PDecayChannel();
    const char** decay_channel_ids = new const char*[mother->daughters.size()];

    int idx = 0;
    for (const auto& part : mother->daughters)
    {
        if (part->pid == -1) { build_recursive_decay(part->mother->name.c_str(), part->name.c_str() + 5, pdm); }
        else
        {
            decay_channel_ids[idx++] = part->name.c_str();

            if (part->daughters.size() > 0) { compile_reactions(part.get(), pdm, level + 1); }
        }
        // printf("%*c   part id:  %4d  %s  %d\n", level+1, ' ', part->pid, part->name.c_str());
    }

    if (idx)
    {
        decay_channels->AddChannel(1.0, idx, decay_channel_ids);

        if (mother->mother)
        {
            // printf("%*c   -> BR: ", level+1, ' ');
            // decay_channels->Print();
            // printf("\n");
            pdm->AddChannel(mother->name.c_str(), decay_channels);
            // delete decay_channels; FIXME
            // decay_channels = nullptr;
        }
    }
    delete[] decay_channel_ids;

    return decay_channels;
}

/// The database channel record
struct channel_data
{
    int id;             ///< channel id
    float width;        ///< decay width
    float crosssection; ///< channel cross-section
    std::string body;   ///< the reaction body
};

auto load_database(const char* dbfile) -> std::map<int, channel_data>
{
    std::map<int, channel_data> channels;

    std::ifstream ofs(dbfile, std::ios::in);

    if (ofs.is_open())
    {
        std::string line;
        while (std::getline(ofs, line))
        {
            trim(line);
            if (line.length() == 0) { continue; }
            if (line[0] == '#') { continue; }
            if (line[0] == '@')
            {
                init_collision_system(line.c_str() + 1);
                continue;
            }

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
            replace_chars(_parts, '#', ',');
            clear_chars(_parts, '[');
            clear_chars(_parts, ']');

            channel_data chd = {channel, width, crosssection, particles};
            if (channels.find(channel) != channels.end())
            {
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

auto init_custom_strangeness() -> void {
    if (makeStaticData()->AddParticle(-1, "Lambda1385", 1.385) > 0) {
        makeStaticData()->AddAlias("Lambda1385", "Lambda(1385)");

        makeStaticData()->SetParticleTotalWidth("Lambda1385", 0.05);
        makeStaticData()->SetParticleBaryon("Lambda1385", 1);
        makeStaticData()->SetParticleSpin("Lambda1385", 1);
        makeStaticData()->SetParticleParity("Lambda1385", -1);

        makeStaticData()->AddDecay("Lambda(1385) --> Sigma+ + pi-",   "Lambda1385", "Sigma+,pi-", 1. / 3);
        makeStaticData()->AddDecay("Lambda(1385) --> Sigma- + pi+",   "Lambda1385", "Sigma-,pi+", 1. / 3);
        makeStaticData()->AddDecay("Lambda(1385) --> Sigma0 + pi0",   "Lambda1385", "Sigma0,pi0", 1. / 3);

        makeStaticData()->AddDecay("Lambda(1385) --> Lambda + gamma", "Lambda1385", "Lambda,g",   0.0085);
        makeStaticData()->AddDecay("Lambda(1385) --> Sigma(1385)0 + gamma", "Lambda1385", "Sigma13850,g",   0.0085);

        makeStaticData()->AddDecay("Lambda(1385) --> Lambda + dilepton", "Lambda1385", "Lambda,dilepton", 0.0085 / 137.);

        PResonanceDalitz * dalitz = new PResonanceDalitz("Lambda1385_dalitz@Lambda1385_to_Lambda_dilepton", "dgdm from Zetenyi/Wolf", -1);
        dalitz->setGm(0.719);
        makeDistributionManager()->Add(dalitz);
    } else {
        Error("ExecCommand", "PIDs blocked, plugin disabled");
    }
}

auto init_nstar() -> void
{
#ifdef NSTARS
#ifdef LOOP_DEF
    const int nstar_cnt = 8;

    int nstar_num[nstar_cnt] = {1650, 1710, 1720, 1875, 1880, 1895, 1900, 2190};
    float nstar_mass[nstar_cnt] = {1.655, 1.710, 1.720, 1.875, 1.870, 1.895, 1.900, 2.190};
    float nstar_width[nstar_cnt] = {0.150, 0.200, 0.250, 0.220, 0.235, 0.090, 0.250, 0.0250};

    for (int i = 0; i < nstar_cnt; ++i)
    {
        char* res_decay = new char[200];
        char* res_name = new char[200];

        sprintf(res_name, "NStar%d", nstar_num[i]);
        sprintf(res_decay, "%s --> Lambda + K+", res_name);
        printf("--- [%d] Adding resonance id = %d: %s : %s\n", i, nstar_num[i], res_name, res_decay);
        pid_resonance = makeStaticData()->AddParticle(-1, res_name, nstar_mass[i]); // Mass in GeV/c2
        printf("   PID resonance: %d\n", pid_resonance);
        makeStaticData()->SetParticleTotalWidth(res_name, nstar_width[i]);
        makeStaticData()->SetParticleBaryon(res_name, 1);
        decay_index = makeStaticData()->AddDecay(res_decay, res_name, "Lambda, K+", 1);
        listParticle(res_name);
    }
#else

    {
        pid_resonance = makeStaticData()->AddParticle(-1, "NStar1650", 1.650); // Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar1650", 0.100);
        makeStaticData()->SetParticleBaryon("NStar1650", 1);
        decay_index = makeStaticData()->AddDecay(" NStar1650--> Lambda + K+", "NStar1650", "Lambda, K+", 1);
        listParticle("NStar1650");
    }

    {
        pid_resonance = makeStaticData()->AddParticle(-1, "NStar1710", 1.710); // Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar1710", 0.2);
        makeStaticData()->SetParticleBaryon("NStar1710", 1);
        decay_index = makeStaticData()->AddDecay("NStar1710 --> Lambda + K+", "NStar1710", "Lambda, K+", 1);
        listParticle("NStar1710");
    }

    {
        pid_resonance = makeStaticData()->AddParticle(-1, "NStar1720", 2.000); // Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar1720", 0.2);
        makeStaticData()->SetParticleBaryon("NStar1720", 1);
        decay_index = makeStaticData()->AddDecay("NStar1720 --> Lambda + K+", "NStar1720", "Lambda, K+", 1);
        listParticle("NStar1720");
    }

    {
        pid_resonance = makeStaticData()->AddParticle(-1, "NStar1900", 2.400); // Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar1900", 0.200);
        makeStaticData()->SetParticleBaryon("NStar1900", 1);
        decay_index = makeStaticData()->AddDecay("NStar1900 --> Lambda + K+", "NStar1900", "Lambda, K+", 1);
        listParticle("NStar1900");
    }

    {
        pid_resonance = makeStaticData()->AddParticle(-1, "NStar2190", 2.80); // Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar2190", 0.200);
        makeStaticData()->SetParticleBaryon("NStar2190", 1);
        decay_index = makeStaticData()->AddDecay("NStar2190 --> Lambda + K+", "NStar2190", "Lambda, K+", 1);
        listParticle("NStar2190");
    }

#endif

#endif /* NSTARS */
}

auto run_channel(int channel_id, std::string channel_string, int events, int seed, int loops, std::string output_dir) -> void
{
    auto list_strange = false;
    if (list_strange)
    {
        listParticle("Lambda");
        listParticle("Sigma0");
        listParticle("Sigma+");
        listParticle("Sigma-");
        listParticle("Xi0");
        listParticle("Xi-");
        listParticle("Sigma13850");
        listParticle("Sigma1385+");
        listParticle("Sigma1385-");
        listParticle("Lambda1385");
        listParticle("Lambda1405");
        listParticle("Lambda1520");
        listParticle("Xi15300");
        listParticle("Xi1530-");
    }

    printf("******************* selected channel: %d ***************************\n", channel_id);

    for (int l = seed; l < seed + loops; ++l)
    {
        PUtils::SetSeed(l);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        char buff[200];
        if (channel_id < 0) { sprintf(buff, "pluto_chan_%s_events_%d_seed_%04d", channel_string.c_str(), events, l); }
        else { sprintf(buff, "pluto_chan_%03d_events_%d_seed_%04d", channel_id, events, l); }
        std::string tmpname = buff;

        if (output_dir.length()) tmpname = output_dir + "/" + tmpname;

            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            // Some Particles for the reaction

#ifndef NSTARS
        check_collision_system();

        PDecayManager* pdm = new PDecayManager;

        size_t sta = 0;
        reaction_node reaction_mother;

        parse_reaction(channel_string, &reaction_mother, sta, 0, verbose_flag);

        if (verbose_flag)
        {
            print_reactions(&reaction_mother);
            putchar('\n');
        }

        auto decay_channels = compile_reactions(&reaction_mother, pdm);

        pdm->InitReaction(q, decay_channels);
        pdm->Print();

        /*Int_t n = */ pdm->loop(events, 0, const_cast<char*>(tmpname.c_str()), 0, 0, 0, 1, 1);

        delete decay_channels;
        // delete pdm; FIXME Why it causes double free or corruption error?
#else

        char str_en[20];
        sprintf(str_en, "%f", Eb);
        std::string _part = particles;
        replace_chars(_part, '@', ' ');
        replace_chars(_part, ',', ' ');

        PReaction my_reaction(str_en, "p", "p", const_cast<char*>(_part.c_str()), const_cast<char*>(tmpname.c_str()), 0, 0, 0, 1);
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
auto query_channel(int channel_id, std::string channel_string) -> bool
{
    check_collision_system();

    size_t sta = 0;
    reaction_node reaction_mother;

    parse_reaction(channel_string, &reaction_mother, sta, 0, verbose_flag);

    // q->Print();

    float total_mass = 0.;
    for (const auto& part : reaction_mother.daughters)
    {
        auto mass = makeStaticData()->GetParticleMass(part->pid);
        // printf("part %d  %s  m=%f MeV/c2\n", part->pid, part->name.c_str(), mass);
        total_mass += mass;
    }

    printf("Channel %4d :", channel_id);
    printf("  sqrt(s) = %f  (%s + %s @ %g GeV)", q->M(), p_b->Name(), p_t->Name(), beam_energy);
    printf("  M = %f %c  ", total_mass, total_mass > q->M() ? '!' : ' ');
    print_reactions(&reaction_mother);
    putchar('\n');

    return q->M() > total_mass;
}

auto usage(char** argv, int def_events, int def_seed, int def_loops, const char * def_output) -> void {
    printf("Usage: %s [options] [args]\n\n  where options are:\n"
           "    --verbose                      : make verbose output\n"
           "    --brief                        : make output bries\n"
           "\n"
           "    -d file, --database file       : databse file, default is ./ChannelsDatabase.txt\n"
           "    --query                        : query selected channels\n"
           "    -c pattern --collision pattern : collision pattern in form of part_1,part_2,kinetic_energy\n"
           "    -e number, --events number     : number of events, default is %d\n"
           "    -s seed, --seed seed           : reaction seed, default is %d\n"
           "    -l loops, --loops loops        : number of loops to execute, each loop increases seed by 1, default is %d\n"
           "    -T energy, --energy energy     : override kinetic energy of the beam\n"
           "    -o dir, --output output_dir    : output directory to save files, default is %s\n"
           "    -h, --help                     : this help\n"
           "\n"
           "  and arguments are:\n"
           "    channel_number                 : channel numbers, can specify multiple channels\n"
           "    reaction_body                  : the reaction string, not allowed in --query mode\n",
           argv[0], def_events, def_seed, def_loops, def_output);
}

int main(int argc, char** argv)
{
    int par_random_seed = 0;
    int par_events = 10000;
    int par_loops = 1;
    int par_scale = 0;
    int par_seed = 0;
    int par_query = 0;
    int list_strange = 0;
    float Eb = 0.0;
    const char* par_system = nullptr;

    std::string par_database = "ChannelsDatabase.txt";
    std::string par_output = "";

    struct option long_options[] = {{"verbose", no_argument, &verbose_flag, 1},
                                    {"brief", no_argument, &verbose_flag, 0},
                                    {"random-seed", no_argument, &par_random_seed, 1},
                                    {"list-strange", no_argument, &list_strange, 1},
                                    {"query", no_argument, &par_query, 1},
                                    {"collision", required_argument, 0, 'c'},
                                    {"database", required_argument, 0, 'd'},
                                    {"events", required_argument, 0, 'e'},
                                    {"help", no_argument, 0, 'h'},
                                    {"loops", required_argument, 0, 'l'},
                                    {"output", required_argument, 0, 'o'},
                                    {"seed", required_argument, 0, 's'},
                                    {"energy", required_argument, 0, 'T'},
                                    {"scale", required_argument, 0, 'x'},
                                    {0, 0, 0, 0}};

    Int_t c = 0;
    while (1)
    {
        int option_index = 0;

        c = getopt_long(argc, argv, "c:d:e:hl:o:s:T:x:", long_options, &option_index);
        if (c == -1) break;

        switch (c)
        {
            case 0:
                if (long_options[option_index].flag != 0) break;
                printf("option %s", long_options[option_index].name);
                if (optarg) printf(" with arg %s", optarg);
                printf("\n");
                break;
            case 'c':
                par_system = optarg;
                break;
            case 'd':
                par_database = optarg;
                break;
            case 'e':
                par_events = atoi(optarg);
                break;
            case 'h':
                usage(argv, par_events, par_seed, par_loops, par_output.c_str());
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
            case 'T':
                Eb = atof(optarg);
                break;
            case '?':
                //                 abort();
                break;
            default:
                break;
        }
    }

    if (par_system) { init_collision_system(par_system); }
    else if (Eb > 0.0) { init_collision_system(nullptr, nullptr, Eb); }

    //PStrangenessPlugin::EnableExperimentalDecays(true);
    makeDistributionManager()->Exec("strangeness:init");
    // PMesonsPlugin::EnableExperimentalDecays(kTRUE);
    makeDistributionManager()->Exec("mesons:init");

    init_custom_strangeness();

    if (par_query)
    {
        auto channels = load_database(par_database.c_str());

        if (optind == argc)
        {
            for (const auto& channel : channels)
                query_channel(channel.first, channel.second.body);
            exit(EXIT_SUCCESS);
        }
        else
        {
            while (optind < argc)
            {
                try
                {
                    auto selected_channel = stoi(argv[optind]);

                    const auto chit = channels.find(selected_channel);
                    if (chit == channels.end()) { fprintf(stderr, "Channel %d is missing\n", selected_channel); }

                    if (!query_channel(selected_channel, chit->second.body)) exit(EXIT_FAILURE);
                }
                catch (const std::invalid_argument&)
                {
                    fprintf(stderr, "Argument must be a channel number, skipping it.\n");
                }
                optind++;
            }
        }

        exit(EXIT_SUCCESS);
    }
    else
    {
        if (optind == argc)
        {
            std::cerr << "No channel or reaction given! Exiting..." << std::endl;
            std::exit(EXIT_FAILURE);
        }

        bool db_loaded = false;
        decltype(load_database({})) channels;

        while (optind < argc)
        {
            try
            {
                auto selected_channel = stoi(argv[optind]);

                if (!db_loaded)
                {
                    channels = load_database(par_database.c_str());
                    db_loaded = true;
                }

                const auto chit = channels.find(selected_channel);
                if (chit == channels.end())
                {
                    printf("Channel %d is missing\n", selected_channel);
                    exit(EXIT_FAILURE);
                }

                run_channel(selected_channel, chit->second.body, par_events, par_seed, par_loops, par_output);
            }
            catch (const std::invalid_argument&)
            {
                run_channel(-1, argv[optind], par_events, par_seed, par_loops, par_output);
            }
            optind++;
        }

        exit(EXIT_SUCCESS);
    }
}
