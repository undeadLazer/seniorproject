#pragma once

#include <iostream>
#include "tree_summary.hpp"
#include "data.hpp"
#include "likelihood.hpp"
#include "lot.hpp"
#include "chain.hpp"
#include "gamma_shape_updater.hpp"
#include <boost/program_options.hpp>
#include "output_manager.hpp"

namespace phylocuda {

class THelper
    {
    public:
                                 THelper();
                                    ~THelper();

        void                        clear();
        void                        processCommandLineOptions(int argc, const char * argv[]);
        void                        run();

    private:

        std::string                 _data_file_name;
        std::string                 _tree_file_name;

        double                      _expected_log_likelihood;
        double                      _gamma_shape;
        unsigned                    _num_categ;
        std::vector<double>         _state_frequencies;
        std::vector<double>         _exchangeabilities;

        Data::SharedPtr             _data;
        Model::SharedPtr            _model;
        Likelihood::SharedPtr       _likelihood;
        TreeSummary::SharedPtr      _tree_summary;
        Lot::SharedPtr              _lot;

        unsigned                    _random_seed;
        unsigned                    _num_iter;
        unsigned                    _sample_freq;

        void                        sample(unsigned iter, Chain & chain);
        OutputManager::SharedPtr    _output_manager;

        static std::string          _program_name;
        static unsigned             _major_version;
        static unsigned             _minor_version;

    };

inline THelper::THelper()
    {
    //std::cout << "Constructing a THelper" << std::endl;
    clear();
    }

inline THelper::~THelper()
    {
    //std::cout << "Destroying a THelper" << std::endl;
    }

inline void THelper::clear()
    {
    _data_file_name = "";
    _tree_file_name = "";
    _data           = nullptr;
    _model          = nullptr;
    _likelihood     = nullptr;
    _tree_summary   = nullptr;
    _lot            = nullptr;
    _expected_log_likelihood = 0.0;
    _gamma_shape = 0.5;
    _num_categ = 1;
    _state_frequencies.resize(0);
    _exchangeabilities.resize(0);
    _random_seed     = 1;
    _num_iter        = 1000;
    _sample_freq     = 1;
    _output_manager = nullptr;
    }

inline void THelper::processCommandLineOptions(int argc, const char * argv[])
    {
    boost::program_options::variables_map       vm;
    boost::program_options::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("version,v", "show program version")
        ("seed,z",        boost::program_options::value(&_random_seed)->default_value(1),   "pseudorandom number seed")
        ("niter,n",       boost::program_options::value(&_num_iter)->default_value(1000),   "number of MCMC iterations")
        ("samplefreq",  boost::program_options::value(&_sample_freq)->default_value(1),   "skip this many iterations before sampling next")
        ("datafile,d",  boost::program_options::value(&_data_file_name)->required(), "name of data file in NEXUS format")
        ("treefile,t",  boost::program_options::value(&_tree_file_name)->required(), "name of data file in NEXUS format")
        ("expectedLnL", boost::program_options::value(&_expected_log_likelihood)->default_value(0.0), "log likelihood expected")
        ("gammashape,s", boost::program_options::value(&_gamma_shape)->default_value(0.5), "shape parameter of the Gamma among-site rate heterogeneity model")
        ("ncateg,c",     boost::program_options::value(&_num_categ)->default_value(1),     "number of categories in the discrete Gamma rate heterogeneity model")
        ("statefreq,f",  boost::program_options::value(&_state_frequencies)->multitoken()->default_value(std::vector<double> {0.25, 0.25, 0.25, 0.25}, "0.25 0.25 0.25 0.25"),  "state frequencies in the order A C G T (will be normalized to sum to 1)")
        ("rmatrix,r",    boost::program_options::value(&_exchangeabilities)->multitoken()->default_value(std::vector<double> {1, 1, 1, 1, 1, 1}, "1 1 1 1 1 1"),                "GTR exchangeabilities in the order AC AG AT CG CT GT (will be normalized to sum to 1)")
        ;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
    try
        {
        const boost::program_options::parsed_options & parsed = boost::program_options::parse_config_file< char >("phylocuda.conf", desc, false);
        boost::program_options::store(parsed, vm);
        }
    catch(boost::program_options::reading_file & x)
        {
        std::cout << "Note: configuration file (phylocuda.conf) not found" << std::endl;
        }
    boost::program_options::notify(vm);

    // If user specified --help on command line, output usage summary and quit
    if (vm.count("help") > 0)
        {
        std::cout << desc << "\n";
        std::exit(1);
        }

    // If user specified --version on command line, output version and quit
    if (vm.count("version") > 0)
        {
        std::cout << boost::str(boost::format("This is %s version %d.%d") % _program_name % _major_version % _minor_version) << std::endl;
        std::exit(1);
        }

    // Be sure state frequencies sum to 1.0 and are all positive
    double sum_freqs = std::accumulate(_state_frequencies.begin(), _state_frequencies.end(), 0.0);
    for (auto & freq : _state_frequencies)
        {
        if (freq <= 0.0)
            throw eX("all statefreq entries must be positive real numbers");
        freq /= sum_freqs;
        }

    // Be sure exchangeabilities sum to 1.0 and are all positive
    double sum_xchg = std::accumulate(_exchangeabilities.begin(), _exchangeabilities.end(), 0.0);
    for (auto & xchg : _exchangeabilities)
        {
        if (xchg <= 0.0)
            throw eX("all rmatrix entries must be positive real numbers");
        xchg /= sum_xchg;
        }

    // Be sure gamma shape parameter is positive
    if (_gamma_shape <= 0.0)
        throw eX("gamma shape must be a positive real number");

    // Be sure number of gamma rate categories is greater than or equal to 1
    if (_num_categ < 1)
        throw eX("ncateg must be a positive integer greater than 0");

    }

inline void THelper::sample(unsigned iteration, Chain & chain)
    {
    if (chain.getHeatingPower() == 1 && iteration % _sample_freq == 0)
        {
        double logLike = chain.calcLogLikelihood();
        double logPrior = chain.calcLogJointPrior();
        double TL = chain.getTreeManip()->calcTreeLength();
        _output_manager->outputConsole(boost::str(boost::format("%12d %12.5f %12.5f %12.5f") % iteration % logLike % logPrior % TL));
        _output_manager->outputTree(iteration, chain.getTreeManip());
        _output_manager->outputParameters(iteration, logLike, logPrior, TL, chain.getModel());
        }
    }

inline void THelper::run()
    {
    std::cout << "Starting..." << std::endl;

    try
        {
        // Read and store data
        _data = Data::SharedPtr(new Data());
        _data->getDataFromFile(_data_file_name);

        // Create a substitution model
        _model = Model::SharedPtr(new Model());
        _model->setExchangeabilitiesAndStateFreqs(_exchangeabilities, _state_frequencies);
        _model->setGammaShape(_gamma_shape);
        _model->setGammaNCateg(_num_categ);
        std::cout << _model->describeModel() << std::endl;

        // Create a Likelihood object that will compute log-likelihoods
        _likelihood = Likelihood::SharedPtr(new Likelihood());
        _likelihood->setData(_data);
        _likelihood->setModel(_model);

        // Read in a tree
        _tree_summary = TreeSummary::SharedPtr(new TreeSummary());
        _tree_summary->readTreefile(_tree_file_name, 0);
        Tree::SharedPtr tree = _tree_summary->getTree(0);
        std::string newick = _tree_summary->getNewick(0);

        // Calculate the log-likelihood for the tree
        double lnL = _likelihood->calcLogLikelihood(tree);
        std::cout << boost::str(boost::format("log likelihood = %.5f") % lnL) << std::endl;
        if (_expected_log_likelihood != 0.0)
            std::cout << boost::str(boost::format("      (expecting %.5f)") % _expected_log_likelihood) << std::endl;

        // Create a Lot object that generates (pseudo)random numbers
        _lot = Lot::SharedPtr(new Lot);
        _lot->setSeed(_random_seed);

        // Create an output manager and open output files
        _output_manager.reset(new OutputManager);
        _output_manager->outputConsole(boost::str(boost::format("\n%12s %12s %12s %12s") % "iteration" % "logLike" % "logPrior" % "TL"));
        _output_manager->openTreeFile("trees.tre", _data);
        _output_manager->openParameterFile("params.txt", _likelihood->getModel());

        // Create a Chain object and take _num_iter steps
        Chain chain;
        chain.setLot(_lot);
        chain.setLikelihood(_likelihood);
        chain.setTreeFromNewick(newick);
        chain.start();
        sample(0, chain);
        for (unsigned iteration = 1; iteration <= _num_iter; ++iteration)
            {
            chain.nextStep(iteration);
            sample(iteration, chain);
            }
        chain.stop();

        // Close output files
        _output_manager->closeTreeFile();
        _output_manager->closeParameterFile();

        }
    catch (eX & x)
        {
        std::cerr << "THelper encountered a problem:\n  " << x.what() << std::endl;
        }

    std::cout << "\nFinished!" << std::endl;
    }

}
