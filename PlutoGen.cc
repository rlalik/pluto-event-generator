#ifndef __CINT__

#include <PParticle.h>
#include <PData.h>
#include <PStaticData.h>
#include <PChannel.h>
#include <PFireball.h>
#include <PReaction.h>
#include <PDecayChannel.h>
#include <PDecayManager.h>
#include <PUtils.h>
#include <PFileInput.h>
#include <PFilter.h>
#include <PKinematics.h>
#include <PResonanceDalitz.h>
#include <TApplication.h>

#include <TROOT.h>

#endif

#define PR(x) std::cout << "++DEBUG: " << #x << " = |" << x << "| (" << __FILE__ << ", " << __LINE__ << ")\n";

#include <list>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <iomanip>

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

bool find_decay(const std::string line, size_t spos, size_t epos, size_t & dspos)
{
    printf("  +++   find decay: %s\n", line.substr(spos, string::npos).c_str());
    dspos = line.find('[', spos);
    if (dspos < epos)
        return true;

    return false;
}

typedef std::list<PChannel *> PartChain;
typedef std::vector<PParticle *> Particles;
/*
void print_pc(const PartChain p_c)
{
    printf("###(%d) -> ", p_c.size());
    for (int i = 0; i < p_c.size(); ++i)
         printf("%s ", ((PParticle *)p_c[i])->Name() );
    printf("\n");
}

void print_p(PParticle ** p_c, int n)
{
    printf("+++(%d) -> ", n);
    for (int i = 0; i < n; ++i)
         printf("%s ", ((PParticle *)p_c[i])->Name() );
    printf("\n");
}
*/
Particles parse_reaction(const std::string line, size_t & spos, size_t epos, PartChain & p_c)
{
    Particles parts;

    static int level = 0;
    ++level;
    //printf("+++%d parse string: %s\n", level, line.substr(spos, epos).c_str());
    size_t & next_stop = spos;
    size_t this_stop = spos;
    size_t decay_start = 0;
    size_t decay_stop = 0;

    while (next_stop != epos and next_stop != string::npos)
    {
        next_stop = line.find_first_of(",]", this_stop);

        bool ret = find_decay(line, this_stop, next_stop, decay_start);
        //printf("        decay result = %d in %lu-%lu\n", ret, decay_start, decay_stop);
        if (ret == true)
        {
            std::string c_name = line.substr(this_stop, decay_start-this_stop).c_str();

            next_stop = decay_start+1;
            Particles _parts = parse_reaction(line, next_stop, decay_stop-2, p_c);

            //printf("after %s\n", c_name.c_str());
            size_t n_in_decay = _parts.size();
            PParticle ** pp = new PParticle*[n_in_decay+1];
            pp[0] = new PParticle(c_name.c_str());
            //pp[0]->Print();
            for (uint i = 0; i < n_in_decay; ++i)
            {
                pp[i+1] = (PParticle *)_parts[i];
                //pp[i+1]->Print();
            }
            PChannel * channel = new PChannel(pp, n_in_decay);
            p_c.push_front(channel);
            parts.push_back(pp[0]);
        }
        else
        {
            std::string c_name = line.substr(this_stop, next_stop-this_stop);
            //printf("+++ found particle: %s\n", line.substr(this_stop, next_stop-this_stop).c_str());
            PParticle * p = new PParticle(c_name.c_str());
            parts.push_back(p);
        }
        if (next_stop >= line.length()) { spos = string::npos; return parts; }

        this_stop = next_stop + 1;
        if (line[next_stop] == ']') { spos = this_stop; return parts; }
    }

    --level;
    return parts;
}

#ifndef __CINT__
int main(int argc, char **argv) {
    int verbose_flag = 0;
    int par_random_seed = 0;
    int par_events = 10000;
    int par_loops = 1;
    int par_scale = 0;
    int par_seed = 0;
    Float_t Eb = 4.5;         // beam energy in AGeV

    std::string par_database = "ChannelsDatabase.txt";
    std::string par_output = "";

    struct option long_options[] =
    {
        {"verbose",     no_argument,        &verbose_flag,    1},
        {"brief",       no_argument,        &verbose_flag,    0},
        {"random-seed", no_argument,        &par_random_seed, 1},
        {"energy",      required_argument,  0,                'E'},
        {"database",    required_argument,  0,                'd'},
        {"events",      required_argument,  0,                'e'},
        {"loops",       required_argument,  0,                'l'},
        {"output",      required_argument,  0,                'o'},
        {"scale",       required_argument,  0,                'x'},
        {"seed",        required_argument,  0,                's'},
        {"help",        no_argument,        0,                'h'},
        { 0, 0, 0, 0 }
    };

    Int_t c = 0;
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "E:hd:e:l:o:x:s:", long_options, &option_index);
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
            case 'E':
                Eb = atof(optarg);
                break;
            case 'd':
                par_database = optarg;
                break;
            case 'e':
                par_events = atoi(optarg);
                break;
            case 'l':
                par_loops = atoi(optarg);
                break;
            case 'o':
                par_output = optarg;
                break;
            case 'x':
                par_scale = atoi(optarg);
                break;
            case 's':
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
        break;
    }

    std::vector<std::string> tokens;

    int channel;
    float width;
    float crosssection;
    std::string particles;
    std::string comment;

    std::ifstream ofs(par_database.c_str(), std::ios::in);

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

    makeDistributionManager()->Exec("strangeness:init");

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
    makeStaticData()->SetParticleSpin("Lambda15200",3);
    makeStaticData()->SetParticleParity("Lambda15200",1);

    if (selected_channel == 48)
    {
        makeStaticData()->AddDecay("Lambda(1520)0 --> Lambda + e+ + e-", "Lambda15200", "Lambda, dilepton", .007948/137.);
        PResonanceDalitz * L15200_dalitz = new PResonanceDalitz("Lambda15200_dalitz@Lambda15200_to_Lambda_dilepton","dgdm from Zetenyi/Wolf", -1);
        L15200_dalitz->setGm(0.719);
        makeDistributionManager()->Add(L15200_dalitz);
    }

    if (selected_channel == 49)
    {
        makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma0 + pi0", "Lambda15200", "Sigma0, pi0", .139096); // -
        makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma0 + pi0 + pi0", "Lambda15200", "Sigma0, pi0, pi0", .009/5.0); // -
        makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma0 + pi+ + pi-", "Lambda15200", "Sigma0, pi+, pi-", .009/5.0); // -
        makeStaticData()->AddDecay("Lambda(1520)0 --> Lambda + pi0 + pi0", "Lambda15200", "Lambda, pi0, pi0", .1/3.0); // -
        makeStaticData()->AddDecay("Lambda(1520)0 --> Lambda + g", "Lambda15200", "Lambda, g", .0085);
    }
    listParticle("Lambda15200");

//    makeStaticData()->AddDecay("Lambda(1520)0 --> K- + p", "Lambda15200", "K-,p", .223547 ); +
//    makeStaticData()->AddDecay("Lambda(1520)0 --> K0S + n", "Lambda15200", "K0S,n", .223547 ); +
//    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma+ + pi-", "Lambda15200", "Sigma+, pi-", .139096);
//    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma- + pi+", "Lambda15200", "Sigma-, pi+", .139096);

//    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma0 + g", "Lambda15200", "Sigma0, g", .019373);
//    makeStaticData()->AddDecay("Lambda(1520)0 --> Lambda + pi+ + pi-", "Lambda15200", "Lambda, pi+, pi-", .014638); + BR 0.2/3
//    makeStaticData()->AddDecay("Lambda(1520)0 --> Lambda + e+ + e-", "Lambda15200", "Lambda, dilepton", .007948/137.);

//    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma(1385)+ + pi-", "Lambda15200", "Sigma1385+, pi-", .028780);
//    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma(1385)- + pi+", "Lambda15200", "Sigma1385-, pi+", .028780);
//    makeStaticData()->AddDecay("Lambda(1520)0 --> Sigma(1385)0 + pi0", "Lambda15200", "Sigma13850, pi0", .028780);

    Int_t pid_lambda1405 = makeStaticData()->AddParticle(-1,"Lambda1405z", 1.405);
    makeStaticData()->AddAlias("Lambda1405z","Lambda(1405)z");
    makeStaticData()->SetParticleTotalWidth("Lambda1405z", 0.05);
    makeStaticData()->SetParticleBaryon("Lambda1405z", 1);
    makeStaticData()->SetParticleSpin("Lambda1405z", 1);
    makeStaticData()->SetParticleParity("Lambda1405z", -1);

    if (selected_channel == 50)
    {
        makeStaticData()->AddDecay("Lambda(1405)z -->  Lambda + dilepton", "Lambda1405z", "Lambda, dilepton", 0.0085 / 137. );
        PResonanceDalitz * newmodel = new PResonanceDalitz("Lambda1405z_dalitz@Lambda1405z_to_Lambda_dilepton",
            "dgdm from Zetenyi/Wolf", -1);
        newmodel->setGm(0.719);
        makeDistributionManager()->Add(newmodel);
    }
    if (selected_channel == 51)
    {
        makeStaticData()->AddDecay("Lambda(1405)z -->  pi0 + Sigma0", "Lambda1405z", "pi0, Sigma0", 0.33);
        makeStaticData()->AddDecay("Lambda(1405)z -->  pi+ + Sigma0", "Lambda1405z", "pi+, Sigma-", 0.33);
        makeStaticData()->AddDecay("Lambda(1405)z -->  pi- + Sigma0", "Lambda1405z", "pi-, Sigma+", 0.33);
        makeStaticData()->AddDecay("Lambda(1405)z -->  gamma + Lambda", "Lambda1405z", "g, Lambda", 0.0085);
    }

    listParticle("Lambda1405z");

    Int_t pid_Sigma1385 = makeStaticData()->AddParticle(-1,"Sigma1385z", 1.385);
    makeStaticData()->AddAlias("Sigma1385z","Sigma(1385)z");
    makeStaticData()->SetParticleTotalWidth("Sigma1385z", 0.0395);
    makeStaticData()->SetParticleBaryon("Sigma1385z", 1);
    makeStaticData()->SetParticleSpin("Sigma1385z", 3);
    makeStaticData()->SetParticleParity("Sigma1385z", 1);

    if (selected_channel == 52)
    {
        makeStaticData()->AddDecay("Sigma(1385)z -->  Lambda + dilepton", "Sigma1385z", "Lambda, dilepton", 0.0125 / 137. );
        PResonanceDalitz * newmodel = new PResonanceDalitz("Sigma1385z_dalitz@Sigma1385z_to_Lambda_dilepton",
            "dgdm from Zetenyi/Wolf", -1);
        newmodel->setGm(0.719);
        makeDistributionManager()->Add(newmodel);
    }
    if (selected_channel == 53)
    {
        makeStaticData()->AddDecay("Sigma(1385)z -->  pi0 + Sigma0", "Sigma1385z", "pi0, Sigma0", 0.12/3);
        makeStaticData()->AddDecay("Sigma(1385)z -->  Lambda + pi", "Sigma1385z", "pi0, Lambda", 0.87);
        makeStaticData()->AddDecay("Sigma(1385)z -->  gamma + Lambda", "Sigma1385z", "g, Lambda", 0.0125);
    }

    listParticle("Sigma1385z");

    printf("******************* selected channel: %d ***************************\n", selected_channel);

    for (int l = par_seed; l < par_seed+par_loops; ++l)
    {
        PUtils::SetSeed(l);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        char buff[200];
        if (par_output.length())
            sprintf(buff, "%s/pluto_chan_%03d_events_%d_seed_%04d", par_output.c_str(), channel, par_events, l);
        else
            sprintf(buff, "pluto_chan_%03d_events_%d_seed_%04d", channel, par_events, l);

        std::string tmpname = buff;

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        //Some Particles for the reaction

#ifndef NSTARS
        PParticle * p_b = new PParticle("p", Eb);  //Beam proton
        PParticle * p_t = new PParticle("p");     //Target proton
        PParticle * q = new PParticle(*p_b + *p_t);

        PDecayManager *p_p = new PDecayManager;
        PDecayChannel *c = new PDecayChannel;

        size_t sta = 0;
        PartChain p_c;
        Particles parts = parse_reaction(particles, sta, string::npos, p_c);

        size_t n_in_decay = parts.size();
        PParticle ** pp = new PParticle*[n_in_decay+1];
        pp[0] = q;
        for (uint i = 0; i < n_in_decay; ++i)
            pp[i+1] = (PParticle *)parts[i];
        PChannel * channel = new PChannel(pp, n_in_decay);
        p_c.push_front(channel);

//    print_pc(p_c);

        int size = p_c.size();
        PChannel ** channels = new PChannel*[size];
        PartChain::const_iterator it = p_c.begin();
        int i = 0;
        for (; it != p_c.end(); ++it)
        {
            channels[i++] = (PChannel *) *it;
        }
/*
        PParticle * _dp = new PParticle("D+");
        PParticle * _dp_dl = new PParticle("dilepton");
        PParticle * _dp_p = new PParticle("p");
//        Particle * _dp_decay[] = {_dp, _dp_dl, _dp_p};

        PParticle * _dp_dl_em = new PParticle("e-");
        PParticle * _dp_dl_ep = new PParticle("e+");
//        PParticle * _dp_dl_decay[] = {_dp_dl, _dp_dl_ep, _dp_dl_em};

        PParticle * _p = new PParticle("p");
        PParticle * _pip = new PParticle("pi+");
        PParticle * _pim = new PParticle("pi-");

        PParticle * s1[] = { q, _dp, _p, _pip, _pim };
        PParticle * s2[] = { _dp, _dp_dl, _dp_p };
        PParticle * s3[] = { _dp_dl, _dp_dl_em, _dp_dl_ep };

        PChannel * c1 = new PChannel(s1, 4);
        PChannel * c2 = new PChannel(s2, 2);
        PChannel * c3 = new PChannel(s3, 2);

        PChannel * c0[] = {c1, c2, c3};
//        PChannel * c0[] = { c1 };

//    PParticle * channels[] = {(PParticle *)_dp_decay, _p, _pip, _pim};
*/
    /*
        int size = tokens.size();
        PParticle ** channels = new PParticle*[size];
        for (int i = 0; i < size; ++i)
        {
            channels[i] = new PParticle(tokens[i].c_str());
            printf("+++ channel: %s\n", tokens[i].c_str());
        }
    */

        p_p->SetDefault((char *)"Lambda1405");
        p_p->SetDefault((char *)"Sigma1385+");
        p_p->SetDefault((char *)"Sigma1385-");
        p_p->SetDefault((char *)"Sigma13850");
        p_p->SetDefault((char *)"Lambda15200");
        p_p->SetDefault((char *)"Lambda1405z");
        p_p->SetDefault((char *)"Sigma1385z");
        p_p->SetDefault((char *)"D++");
        p_p->SetDefault((char *)"D+");
        p_p->SetDefault((char *)"D-");
        p_p->SetDefault((char *)"D0");
        p_p->SetDefault((char *)"rho-");
        p_p->SetDefault((char *)"rho+");
        p_p->SetDefault((char *)"rho0");
        p_p->SetDefault((char *)"NP110");
        p_p->SetDefault((char *)"ND130");
        p_p->SetDefault((char *)"NS110");
        p_p->SetDefault((char *)"NP11+");
        p_p->SetDefault((char *)"ND13+");
        p_p->SetDefault((char *)"NS11+");
        p_p->SetDefault((char *)"w");
        p_p->SetDefault((char *)"eta'");
        p_p->SetDefault((char *)"dimuon");
        p_p->SetDefault((char *)"dilepton");
        p_p->SetDefault((char *)"phi");
//        p_p->SetDefault((char *)"K0*896");
        p_p->SetDefault((char *)"Delta2050++");

//    width = 1.0;
        //c->AddChannel(width,size,channels);
//    c->AddChannel(1.0, 4, s1);
///    c->AddChannel(1.0, 2, s2);

//    PReaction my_reaction(c0, 3, 1<<3, 0, const_cast<char *>(tmpname.c_str()) );
        PReaction my_reaction(channels, i/*was 3*/, 1<<3, 0, const_cast<char *>(tmpname.c_str()) );

//        my_reaction.Print();
//        my_reaction.Print();
        my_reaction.loop(par_events);

/*      p_p->InitReaction(q,c);              // initialize the reaction

        p_p->loop(
                par_scale > 0 ? par_scale * crosssection : par_events,
                0,
                const_cast<char *>(tmpname.c_str()), 0, 0, 0, 1, 1
                );
*/
#else

        char str_en[20];
        sprintf(str_en, "%f", Eb);
        std::string _part = particles;
        replace_chars(_part, '@', ' ');
        replace_chars(_part, ',', ' ');

        printf("making reaction: p+p -> %s\n", _part.c_str());

        PReaction my_reaction(str_en, "p", "p", const_cast<char *>(_part.c_str()), const_cast<char *>(tmpname.c_str()), 0, 0, 0, 1);
        my_reaction.Print();
        my_reaction.loop(par_events);
#endif
    }

    printf("Generator finished its job, good bye!\n");
    return 0;
}
#endif
