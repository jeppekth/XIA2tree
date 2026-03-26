//
// Created by Vetle Wegner Ingeberg on 06/03/2026.
//

#include "Sort.h"

#include "TTreeManager.h"
#include "ParticleRange.h"

using namespace Task;

Detector_Histograms_t::Detector_Histograms_t(Histograms &hm, const std::string &name, const size_t &num)
    : time( hm.Create2D(std::string("time_"+name), std::string("Time spectra "+name), 30000, -1500, 1500, "Time [ns]", num, 0, num, std::string(name+" ID")) )
    , time_CFDfail( hm.Create2D(std::string("time_"+name+"_CFDfail"), std::string("Time spectra"+name+" CFD fail"), 30000, -1500, 1500, "Time [ns]", num, 0, num, std::string(name+" ID")) )
    , energy( hm.Create2D(std::string("energy_"+name), std::string("Energy spectra "+name), 65536, 0, 65536, "Energy [ch]", num, 0, num, std::string(name+" ID")) )
    , energy_cal( hm.Create2D(std::string("energy_cal_"+name), std::string("energy spectra "+name+" (cal)"), 16384, 0, 16384, "Energy [keV]", num, 0, num, std::string(name+" ID")) )
    , mult( hm.Create1D(std::string("mult_"+name), std::string("Multiplicity " + name), 128, 0, 128, "Multiplicity") )
{}

void Detector_Histograms_t::Fill(const Entry_t &word)
{
    energy->Fill(word.adcvalue, word.detectorID);
    energy_cal->Fill(word.energy, word.detectorID);
}

void Detector_Histograms_t::Fill(const subvector<Entry_t> &subvec,
                                 const Entry_t *start)
{
    mult->Fill(subvec.size());
    for ( auto &entry : subvec ){
        energy->Fill(entry.adcvalue, entry.detectorID);
        energy_cal->Fill(entry.energy, entry.detectorID);

        if ( start && !( (entry.type == start->type)&&(entry.detectorID == start->detectorID)&&(entry.timestamp==start->timestamp)) ){
            if ( entry.cfdfail ) {
                time_CFDfail->Fill(double(entry.timestamp - start->timestamp) + (entry.cfdcorr - start->cfdcorr), entry.detectorID + 0.5);
            } else {
                time->Fill(double(entry.timestamp - start->timestamp) + (entry.cfdcorr - start->cfdcorr), entry.detectorID + 0.5);
            }
        }
    }
}

HistManager::HistManager(Histograms &histograms, const OCL::UserConfiguration &user_config, const char *custom_sort)
        : configuration( user_config )
        , labr( histograms, "labr", NUM_LABR_DETECTORS )
        , labr_energy_gated( histograms.Create2D("labr_energy_gated", "Uncalibrated LaBr3 - particle gated",
                                            65536, 0, 65536, "Energy [ch]",
                                                 NUM_LABR_DETECTORS, 0, NUM_LABR_DETECTORS, "Detector ID") )
        , labr_energy_cal_gated( histograms.Create2D("labr_energy_cal_gated", "Calibrated LaBr3 - particle gated",
                                            32768, 0, 32768, "Energy [keV]",
                                            NUM_LABR_DETECTORS, 0, NUM_LABR_DETECTORS, "Detector ID") )
        , chargeIntegrator( histograms.Create1D("chargeIntegrator", "Charge integrator", 86400, 0, 86400, "Time [ns]") )
        , userSort( histograms, configuration, custom_sort )
{
}

Detector_Histograms_t *HistManager::GetSpec(const DetectorType &type)
{
    switch ( type ) {
        case DetectorType::labr : return &labr;
        default : return nullptr;
    }
}

void HistManager::AddEntry(Triggered_event &buffer)
{
    userSort.FillEvent(buffer);
    const auto *trigger = buffer.GetTrigger();

    // We get the qint and increment the time spectrum.
    for ( const auto& Qint : buffer.GetDetector(DetectorType::qint) ) {
        auto time = double(Qint.timestamp) / 1e9; // Convert to second
        chargeIntegrator->Fill(time);
    }

    // For now, we will discard events with bad CFD
    // We have this req. if we get a trigger
    if ( trigger )
        if ( trigger->cfdfail )
            return;

    for ( auto &type : {DetectorType::labr} ){
        GetSpec(type)->Fill(buffer.GetDetector(type), trigger);
    }
}

Sorter::Sorter(TEventQueue_t &input, const OCL::UserConfiguration &config,
               const char *tree_name, const char *user_sort)
    : input_queue( input )
    , histograms( )
    , hm( histograms, config, user_sort )
    , userConfig( config )
    , tree( ( tree_name ) ? new ROOT::TTreeManager(tree_name) : nullptr )
{
}

void Sorter::Run() {
    std::pair<std::vector<Entry_t>, int> entries;
    while ( input_queue.is_not_finish() || !input_queue.empty() ) {
        if ( !input_queue.try_pop(entries) ) {
            std::this_thread::yield();
            continue;
        }
        if ( entries.first.empty() )
            continue;
        if ( userConfig.GetSortType() == CLI::sort_type::gap ){
            if ( entries.second > 0 )
                continue;
            Triggered_event event(entries.first);
            hm.AddEntry(event);
            if ( tree ) tree->Fill(event);
        } else {
            Triggered_event event(entries.first, entries.first[entries.second]);
            hm.AddEntry(event);
            if ( tree ) tree->Fill(event);
        }
    }
    is_done = true;
    tree.reset(nullptr);
}