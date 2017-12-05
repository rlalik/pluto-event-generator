#ifndef __CINT__
/*
#include "PChannel.h"
#include "PData.h"
#include "PDecayChannel.h"
#include "PDecayManager.h"
#include "PDiLepton.h"
#include "PFileInput.h"
#include "PFilter.h"
#include "PFireball.h"
#include "PKinematics.h"
#include "PParticle.h"
#include "PReaction.h"
#include "PThermal.h"
#include "PUtils.h"
#include "TApplication.h"
*/
/*
# include "/home/eepple/hadesdst/function_collection/FAT_hydraheaders.h"
# include "/home/eepple/hadesdst/function_collection/FAT_RootHeaders.h"
# include "/home/eepple/hadesdst/function_collection/FAT_ProtonProtonHeader.h"
# include "/home/eepple/hadesdst/function_collection/FAT_physicfunc.h"
*/
# include <PParticle.h>
# include <PData.h>
# include <PStaticData.h>
# include <PChannel.h>
# include <PFireball.h>
# include <PReaction.h>
# include <PDecayChannel.h>
# include <PDecayManager.h>
# include <PUtils.h>
# include <PFileInput.h>
# include <PFilter.h>
# include <PKinematics.h>
# include <TApplication.h>

#include <TROOT.h>

//using namespace std;

#endif

#define PR(x) std::cout << "++DEBUG: " << #x << " = |" << x << "| (" << __FILE__ << ", " << __LINE__ << ")\n";

#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <iomanip>

#include <getopt.h>

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

#ifndef __CINT__
int main(int argc, char **argv) {
    int verbose_flag = 0;
    int par_random_seed = 0;
    int par_events = 1000000;
    int par_loops = 1;
    int par_loop_offset = 0;
    int par_scale = 0;
    int par_seed = 0;
    Float_t Eb = 3.5;         // beam energy in AGeV

    std::string par_database = "ChannelsDatabase.txt";
    std::string par_output = "output";

    struct option long_options[] =
    {
        {"verbose",     no_argument,        &verbose_flag,    1},
        {"brief",       no_argument,        &verbose_flag,    0},
        {"random-seed", no_argument,        &par_random_seed, 1},
        {"energy",        required_argument,  0,                'E'},
        {"database",    required_argument,  0,                'd'},
        {"events",      required_argument,  0,                'e'},
        {"loops",       required_argument,  0,                'l'},
        {"offset",      required_argument,  0,                'f'},
        {"output",      required_argument,  0,                'o'},
        {"scale",       required_argument,  0,                's'},
        {"seed",        required_argument,  0,                't'},
        {"help",        no_argument,        0,                'h'},
        { 0, 0, 0, 0 }
    };

    Int_t c = 0;
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "hd:e:f:l:o:s:t:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 0:
//                 PR(long_options[option_index].name);
//                 PR(*(long_options[option_index].flag));
                if (long_options[option_index].flag != 0)
                    break;
                printf ("option %s", long_options[option_index].name);
                if (optarg)
                    printf (" with arg %s", optarg);
                printf ("\n");
                break;
            case 'E':
                Eb = atof(optarg);
                break;
            case 'd':
                par_database = optarg;
                break;
            case 'e':
                par_events = atoi(optarg);
                break;
            case 'f':
                par_loop_offset = atoi(optarg);
                break;
            case 'l':
                par_loops = atoi(optarg);
                break;
            case 'o':
                par_output = optarg;
                break;
            case 's':
                par_scale = atoi(optarg);
                break;
            case 't':
                par_seed = atoi(optarg);
                break;
            case 'h':
//                 Usage();
                exit(EXIT_SUCCESS);
                break;
            case '?':
//                 abort();
                break;
            default:
                break;
        }
    }

    if (optind == argc) {
        std::cerr << "No channel given! Exiting..." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    int selected_channel = 0;
    while (optind < argc) {
        selected_channel = atoi(argv[optind++]);
    }

    std::vector<std::string> tokens;

    int channel;
    float width;
    float crosssection;
    std::string particles;
    std::string comment;

    std::ifstream ofs(par_database.c_str(), std::ios::out);

    bool has_data = false;

    if (ofs.is_open()) {
        std::string line;
        while (std::getline(ofs, line)) {
            if (line[0] == '@')
                continue;
            tokens.clear();

            std::istringstream iss(line);
            iss >> channel >> width >> crosssection >> particles;
            std::getline(iss, comment);
            trim(comment);

            std::string _parts = particles;
            replace_chars(_parts, '@', ',');
            clear_chars(_parts, '[');
            clear_chars(_parts, ']');

            tokens = split(_parts, ',');

            if (channel == selected_channel) {
                has_data = true;
                break;
            }
        }
    } else {
        std::cerr << "Database " << par_database << " can not be open! Exiting..." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    if (!has_data) {
        std::cerr << "No data for given channel " << selected_channel << "! Exiting..." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    int pid_resonance = 0;
    int decay_index = 0;

    //#define NSTARS 1
#define LOOP_DEF 1

#ifdef NSTARS
#ifdef LOOP_DEF
    const int nstar_cnt = 8;
//    const char * nstar_arr[nstar_num] = { "NStar1650", "NStar1710", "NStar1720", "NStar1875", "NStar1880", "NStar1895", "NStar1900", "NStar2190" };
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
//        pid_resonance = makeStaticData()->AddParticle(nstar_num[i], res_name, nstar_mass[i]);  //Mass in GeV/c2
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
//    PReaction my_reaction4("49.07","p","p","p NStar1650 [Lambda K+]",outputfilename,0,0,0,1);
        listParticle("NStar1650");
    }

    {
        pid_resonance = makeStaticData()->AddParticle(-1,"NStar1710",1.710); //Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar1710",0.2);
        makeStaticData()->SetParticleBaryon("NStar1710",1);
        decay_index = makeStaticData()->AddDecay("NStar1710 --> Lambda + K+", "NStar1710","Lambda, K+", 1);
//    PReaction my_reaction4("49.07","p","p","p NStar1720 [Lambda K+]",outputfilename,0,0,0,1);
        listParticle("NStar1710");
    }

    {
        pid_resonance = makeStaticData()->AddParticle(-1,"NStar1720",2.000); //Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar1720",0.2);
        makeStaticData()->SetParticleBaryon("NStar1720",1);
        decay_index = makeStaticData()->AddDecay("NStar1720 --> Lambda + K+", "NStar1720","Lambda, K+", 1);
//    PReaction my_reaction4("49.07","p","p","p NStar1720 [Lambda K+]",outputfilename,0,0,0,1);
        listParticle("NStar1720");
    }

    {
        pid_resonance = makeStaticData()->AddParticle(-1,"NStar1900",2.400); //Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar1900",0.200);
        makeStaticData()->SetParticleBaryon("NStar1900",1);
        decay_index = makeStaticData()->AddDecay("NStar1900 --> Lambda + K+", "NStar1900","Lambda, K+", 1);
//    PReaction my_reaction4("49.07","p","p","p NStar1900 [Lambda K+]",outputfilename,0,0,0,1);
        listParticle("NStar1900");
    }

    {
        pid_resonance = makeStaticData()->AddParticle(-1,"NStar2190",2.80); //Mass in GeV/c2
        makeStaticData()->SetParticleTotalWidth("NStar2190",0.200);
        makeStaticData()->SetParticleBaryon("NStar2190",1);
        decay_index = makeStaticData()->AddDecay("NStar2190 --> Lambda + K+", "NStar2190","Lambda, K+", 1);
        //    PReaction my_reaction4("49.07","p","p","p NStar2190 [Lambda K+]",outputfilename,0,0,0,1);
        listParticle("NStar2190");
    }

#endif

#endif /* NSTARS */

    makeDistributionManager()->Exec("strangeness:init");

    //loadData("/home/gu27buz/hadesdst/pp35/Sim/Pluto/missing_particles.dat");

    makeStaticData();
    int dpp_2050_pid = makeStaticData()->AddParticle(-1, "Delta2050++", 2.05);
    makeStaticData()->AddAlias("Delta2050++", "Delta(2050)++");
    makeStaticData()->SetParticleTotalWidth("Delta2050++", 0.25);
    makeStaticData()->SetParticleBaryon("Delta2050++", 1);
    makeStaticData()->AddDecay("Delta(2050)++ --> Sigma(1385)+ + K+", "Delta2050++", "Sigma1385+,K+", 1.0);
    listParticle("Delta2050++");


    int l1520_pid = makeStaticData()->AddParticle(-1,"Lambda15200", 1.5195);
    makeStaticData()->AddAlias("Lambda15200","Lambda(1520)0");
    makeStaticData()->SetParticleTotalWidth("Lambda15200",0.0156);
    makeStaticData()->SetParticleBaryon("Lambda15200",1);
    //makeStaticData()->SetParticleLMass("Lambda15200",1.2);

    makeStaticData()->AddDecay("Lambda(1520)0 --> K- + p", "Lambda15200", "K-,p", .223547 );  
    makeStaticData()->AddDecay("Lambda(1520)0 --> K0S + n", "Lambda15200", "K0S,n", .223547 );   
    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma+ + pi-", "Lambda15200", "Sigma+, pi-", .139096);
    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma- + pi+", "Lambda15200", "Sigma-, pi+", .139096);
    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma0 + pi0", "Lambda15200", "Sigma0, pi0", .139096);
    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma0 + g", "Lambda15200", "Sigma0, g", .019373);
    makeStaticData()->AddDecay("Lambda(1520)0 --> Lambda + pi+ + pi-", "Lambda15200", "Lambda, pi+, pi-", .014638);
    makeStaticData()->AddDecay("Lambda(1520)0 --> Lambda + pi0 + pi0", "Lambda15200", "Lambda, pi0, pi0", .007319);
    makeStaticData()->AddDecay("Lambda(1520)0 --> Lambda + g", "Lambda15200", "Lambda, g", .007948);
    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma(1385)+ + pi-", "Lambda15200", "Sigma1385+, pi-", .028780);
    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma(1385)- + pi+", "Lambda15200", "Sigma1385-, pi+", .028780);
    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma(1385)0 + pi0", "Lambda15200", "Sigma13850, pi0", .028780);

    listParticle("Lambda15200");

/*
0 Lambda1520  1.5195 0.0156 0 0 -1 1 7 1 0 0
  .22500      12014   "Lambda(1520)0 --> K- + p"
  .22500      16013   "Lambda(1520)0 --> K0S + n"
  .14000      9019    "Lambda(1520)0 --> Sigma+ + pi-"
  .14000      8021    "Lambda(1520)0 --> Sigma- + pi+"
  .14000      7020    "Lambda(1520)0 --> Sigma0 + pi0"
  .05000      9008018 "Lambda(1520)0 --> Lambda + pi+ + pi-"
  .05000      7007018 "Lambda(1520)0 --> Lambda + pi0 + pi0"
*/
    for (int l = par_loop_offset; l < par_loops; ++l)
    {
        PUtils::SetSeed(l+par_seed);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        std::stringstream ss;
        ss << par_output;
        //         if (par_loops > 1)
        {
            ss << "_part" << std::setw(3) << std::setfill('0') << l;
        }
        std::string tmpname = ss.str();

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        //Some Particles for the reaction

#ifndef NSTARS
        PParticle * p_b = new PParticle("p", Eb);  //Beam proton
        PParticle * p_t = new PParticle("p");     //Target proton
        PParticle * q = new PParticle(*p_b + *p_t);

        int size = tokens.size();
        PParticle ** channels = new PParticle*[size];
        for (int i = 0; i < size; ++i)
        {
            channels[i] = new PParticle(tokens[i].c_str());
            PR(tokens[i].c_str());
        }

        PDecayManager *p_p = new PDecayManager;
        PDecayChannel *c = new PDecayChannel;

        p_p->SetDefault("Lambda1405");
        p_p->SetDefault("Sigma1385+");
        p_p->SetDefault("Sigma1385-");
        p_p->SetDefault("Sigma13850");
//        p_p->SetDefault("Lambda1520");
        p_p->SetDefault("D++");
        p_p->SetDefault("D+");
        p_p->SetDefault("D-");
        p_p->SetDefault("D0");
        p_p->SetDefault("rho-");
        p_p->SetDefault("rho+");
        p_p->SetDefault("rho0");
        p_p->SetDefault("NP110");
        p_p->SetDefault("ND130");
        p_p->SetDefault("NS110");
        p_p->SetDefault("NP11+");
        p_p->SetDefault("ND13+");
        p_p->SetDefault("NS11+");
        p_p->SetDefault("w");
        p_p->SetDefault("eta'");
        p_p->SetDefault("dimuon");
        p_p->SetDefault("dilepton");
        p_p->SetDefault("phi");
//        p_p->SetDefault("K0*896");
        p_p->SetDefault("Delta2050++");
        p_p->SetDefault("Lambda15200");

        c->AddChannel(width,size,channels);
        //PR(par_events);
        p_p->InitReaction(q,c);              // initialize the reaction


        p_p->loop(
                par_scale > 0 ? par_scale * crosssection : par_events,
                0,
                const_cast<char *>(tmpname.c_str()), 0, 0, 0, 1, 1
                );

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



    return 0;
}
#endif
