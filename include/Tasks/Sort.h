//
// Created by Vetle Wegner Ingeberg on 06/03/2026.
//

#ifndef SORT_H
#define SORT_H

#include <functional>

#include <histogram/ThreadSafeHistograms.h>
#include <UserSort/UserSortManager.h>

#include "RootInterface/TTreeManager.h"

#include "ConfigManager.h"

#include "Task.h"
#include "Queue.h"
#include "event.h"

class ParticleRange;

namespace Task {
    namespace ROOT {
        class TTreeManager;
    }

    struct Detector_Histograms_t
    {
        Histogram2Dp time;
        Histogram2Dp time_CFDfail;
        Histogram2Dp energy;
        Histogram2Dp energy_cal;
        Histogram1Dp mult;

        Detector_Histograms_t(Histograms &hist, const std::string &name, const size_t &num);

        void Fill(const Entry_t &word);
        void Fill(const subvector<Entry_t> &subvec,
                  const Entry_t *start = nullptr);

    };

    class HistManager {
    private:
        const OCL::UserConfiguration configuration;

        Detector_Histograms_t labr;

       
        Histogram2Dp labr_energy_gated, labr_energy_cal_gated;
        Histogram1Dp chargeIntegrator;
        UserSortManager userSort;

        Detector_Histograms_t *GetSpec(const DetectorType &type);

    public:
        HistManager(Histograms &histograms, const OCL::UserConfiguration &configuration,
                    const char *custom_sort = nullptr);
        ~HistManager() = default;

        //! Fill spectra with an event
        void AddEntry(Triggered_event &buffer);

        //! Fill a single word
        //void AddEntry(const Entry_t &word);

        //! Fill spectra directly from iterators
        template<class It>
        inline void AddEntries(It start, It stop){
            using std::placeholders::_1;
            std::for_each(start, stop, [this](const auto &p){ this->AddEntry(p); });
        }

    };

    class Sorter  : public Base
    {
    private:
        Histograms histograms;
        TEventQueue_t &input_queue;
        HistManager hm;
        const OCL::UserConfiguration& userConfig;
        std::unique_ptr<ROOT::TTreeManager> tree;

    public:
        Sorter(TEventQueue_t &input, const OCL::UserConfiguration &config,
             const char *tree_name = nullptr, const char *user_sort = nullptr);
        ~Sorter() override = default;
        void Run() override;
        Histograms &GetHistograms() { return histograms; }
    };
};

#endif // SORT_H