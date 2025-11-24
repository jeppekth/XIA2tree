//
// Created by Vetle Wegner Ingeberg on 20/10/2022.
//

#ifndef XIA2TREE_MTSORT_H
#define XIA2TREE_MTSORT_H

#include <functional>

#include <histogram/ThreadSafeHistograms.h>
#include <UserSort/UserSortManager.h>

#include "ConfigManager.h"

#include "Task.h"
#include "Queue.h"
#include "event.h"
#include "TTreeManager.h"

class ParticleRange;

namespace Task {

    class Sorters;

    namespace ROOT {
        class TTreeManager;
    }

    struct Detector_Histograms_t
    {
        ThreadSafeHistogram2D time;
        ThreadSafeHistogram2D time_CFDfail;
        ThreadSafeHistogram2D energy;
        ThreadSafeHistogram2D energy_cal;
        ThreadSafeHistogram1D mult;

        Detector_Histograms_t(ThreadSafeHistograms &hist, const std::string &name, const size_t &num);

        void Fill(const Entry_t &word);
        void Fill(const subvector<Entry_t> &subvec,
                  const Entry_t *start = nullptr);
        void Flush();

    };

    class HistManager {
    private:
        const OCL::UserConfiguration configuration;

        Detector_Histograms_t labr;

        UserSortManager userSort;

        Detector_Histograms_t *GetSpec(const DetectorType &type);

    public:
        HistManager(ThreadSafeHistograms &histograms, const OCL::UserConfiguration &configuration,
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

        void Flush();
    };

    class MTSort  : public Base
    {
    private:
        TEventQueue_t &input_queue;
        TEventQueue_t &output_queue;
        HistManager hm;
        std::unique_ptr<ROOT::TTreeManager> tree;

    public:
        MTSort(TEventQueue_t &input, TEventQueue_t &output, ThreadSafeHistograms &histograms, const OCL::UserConfiguration &config,
                const char *user_sort = nullptr);
        ~MTSort() override = default;
        void Run() override;
        void Flush();
    };

    class TreeWriter : public Base
    {   
    private:
        TEventQueue_t &input_queue;
        std::unique_ptr<ROOT::TTreeManager> tree;
        bool sorting_finished = false;
        Sorters *sorters;

    public:
        TreeWriter(TEventQueue_t &input, const char *tree_name = nullptr, Sorters *sorters = nullptr)  
        : input_queue(input)
        , tree(new ROOT::TTreeManager(tree_name))
        , sorters(sorters)
        {}
        
        void Run()
        {
            std::pair<std::vector<Entry_t>, size_t> entries;
            while (!done)
            {
                if ( input_queue.wait_dequeue_timed(entries, std::chrono::seconds(1)) )
                {
                    Triggered_event event(entries.first, entries.first[entries.second]);
                    tree->Fill(event);
                }
            }

            is_done = true;
        }

    };

    class Sorters
    {
    private:
        TEventQueue_t &input_queue;
        TEventQueue_t output_queue;
        ThreadSafeHistograms histograms;
        std::vector<MTSort *> sorters;
        const OCL::UserConfiguration &user_config;
        std::string user_sort_path;
        std::string tree_file_name;
        std::vector<std::string> tree_files; //! To be returned to the user when everything is said and done.

    public:
        Sorters(TEventQueue_t &input, OCL::UserConfiguration &config, const char *tree_name = nullptr, const char *user_sort = nullptr);
        ~Sorters();
        void flush();
        Histograms &GetHistograms(){
            flush();
            return histograms.GetHistograms();
        }
        [[nodiscard]] std::vector<std::string> GetTreeFiles() const { return tree_files; }
        MTSort *GetNewSorter();
        TreeWriter *GetNewTreeWriter();

        bool IsFinished()
        {
            for (MTSort * s : sorters) if (!s->check_status()) return false;
            return true;
        }
    };


}

#endif //XIA2TREE_MTSORT_H
