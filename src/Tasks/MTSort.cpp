//
// Created by Vetle Wegner Ingeberg on 20/10/2022.
//

// TODO:
/*
 * 1) Make ede_spectra_bxfy always fill -> good check that data are correctly calibrated
 * 2) Make ede_spectra_gated_fx -> To see with and without the gate
 * 3) Need to be able to pass time gate and
 */

#include "MTSort.h"
#include "TTreeManager.h"
#include "ParticleRange.h"
#include "ConfigManager.h"

using namespace Task;


// For 59Co
/*constexpr double a2[] = {0.000123, 0.000097, 0.000070, 0.000040, 0.000008, -0.000025, -0.000061, -0.000097};
constexpr double a1[] = {-1.033612, -1.032484, -1.031292, -1.030005, -1.028671, -1.027260, -1.025767, -1.024225};
constexpr double a0[] = {15.482874, 15.482970, 15.482859, 15.482356, 15.481656, 15.480574, 15.479027, 15.477125};*/

// For 106Pd
/*constexpr double a2[] = {-0.000004, -0.000020, -0.000038, -0.000057, -0.000077, -0.000099, -0.000121, -0.000146};
constexpr double a1[] = {-1.016472, -1.015782, -1.015050, -1.014245, -1.013415, -1.012532, -1.011625, -1.010621};
constexpr double a0[] = {15.680808, 15.680103, 15.679198, 15.677920, 15.676482, 15.674706, 15.672703, 15.670059};*/

// For 117Sn
constexpr double a2[] = { 0.000126,  0.000115,  0.000103,  0.000091,  0.000078,  0.000065,  0.000050,  0.000035};
constexpr double a1[] = {-1.018782, -1.018272, -1.017734, -1.017168, -1.016573, -1.015948, -1.015291, -1.014599};
constexpr double a0[] = {15.759522, 15.760104, 15.760629, 15.761086, 15.761454, 15.761709, 15.761821, 15.761756};

constexpr double CalcEx(const size_t& ringNo, const double& p_energy) {
    return a0[ringNo] + a1[ringNo] * p_energy + a2[ringNo] * p_energy * p_energy;
}

Detector_Histograms_t::Detector_Histograms_t(ThreadSafeHistograms &hm, const std::string &name, const size_t &num)
    : time( hm.Create2D(std::string("time_"+name), std::string("Time spectra "+name), 30000, -1500, 1500, "Time [ns]", num, 0, num, std::string(name+" ID")) )
    , time_CFDfail( hm.Create2D(std::string("time_"+name+"_CFDfail"), std::string("Time spectra"+name+" CFD fail"), 30000, -1500, 1500, "Time [ns]", num, 0, num, std::string(name+" ID")) )
    , energy( hm.Create2D(std::string("energy_"+name), std::string("Energy spectra "+name), 65536, 0, 65536, "Energy [ch]", num, 0, num, std::string(name+" ID")) )
    , energy_cal( hm.Create2D(std::string("energy_cal_"+name), std::string("energy spectra "+name+" (cal)"), 16384, 0, 16384, "Energy [keV]", num, 0, num, std::string(name+" ID")) )
    , mult( hm.Create1D(std::string("mult_"+name), std::string("Multiplicity " + name), 128, 0, 128, "Multiplicity") )
{}

void Detector_Histograms_t::Fill(const Entry_t &word)
{
    energy.Fill(word.adcvalue, word.detectorID);
    energy_cal.Fill(word.energy, word.detectorID);
}

void Detector_Histograms_t::Fill(const subvector<Entry_t> &subvec,
                                 const Entry_t *start)
{
    mult.Fill(subvec.size());
    for ( auto &entry : subvec ){
        energy.Fill(entry.adcvalue, entry.detectorID);
        energy_cal.Fill(entry.energy, entry.detectorID);

        if ( start && !( (entry.type == start->type)&&(entry.detectorID == start->detectorID)&&(entry.timestamp==start->timestamp)) ){
            if ( entry.cfdfail )
                time_CFDfail.Fill(double(entry.timestamp - start->timestamp) + (entry.cfdcorr - start->cfdcorr), entry.detectorID + 0.5);
            else if ( entry.energy > 1000 && start->energy > 1000)
                time.Fill(double(entry.timestamp - start->timestamp) + (entry.cfdcorr - start->cfdcorr), entry.detectorID + 0.5);
        }
    }
}

void Detector_Histograms_t::Flush()
{
    time.force_flush();
    energy.force_flush();
    energy_cal.force_flush();
    mult.force_flush();
}


HistManager::HistManager(ThreadSafeHistograms &histograms, const OCL::UserConfiguration &user_config, const char *custom_sort)
        : configuration( user_config )
        , labr( histograms, "labr", NUM_LABR_DETECTORS )
        , qint ( histograms, "qint")
       , userSort( histograms, configuration, custom_sort )
{
}

Detector_Histograms_t *HistManager::GetSpec(const DetectorType &type)
{
    switch ( type ) {
        case DetectorType::labr : return &labr;
        case DetectorType::qint : return &qint;
        default : return nullptr;
    }
}

void HistManager::AddEntry(Triggered_event &buffer)
{
    if (buffer.GetEntries().front().type == DetectorType::qint)
    {
        for (Entry_t entry : buffer.GetEntries()) GetSpec(DetectorType::qint)->Fill(entry);
        return;
    }

    auto trigger = buffer.GetTrigger();

    // For now, we will discard events with bad CFD
    // We have this req. if we get a trigger
    if ( trigger )
        if ( trigger->cfdfail )
            return;

    for ( auto &type : {DetectorType::labr} ){
        GetSpec(type)->Fill(buffer.GetDetector(type), trigger);
    }

    userSort.FillEvent(buffer);
}

void HistManager::Flush()
{
    for ( auto &type : {DetectorType::labr} ){
        GetSpec(type)->Flush();
    }

    userSort.Flush();
}

MTSort::MTSort(TEventQueue_t &input, TEventQueue_t &output, ThreadSafeHistograms &histograms, const OCL::UserConfiguration &config, const char *user_sort)
    : input_queue( input )
    , output_queue ( output )
    , hm( histograms, config, user_sort )
{
}

void MTSort::Run()
{
    std::pair<std::vector<Entry_t>, size_t> entries;

    while ( !done ){

        if ( input_queue.wait_dequeue_timed(entries, std::chrono::seconds(1)) ){
            if ( entries.second == -1 || entries.second == -2){
                Triggered_event event(entries.first);
                hm.AddEntry(event);
            }

            Triggered_event event(entries.first, entries.first[entries.second]);
            hm.AddEntry(event);

            while ( !output_queue.try_enqueue(entries) ) if (done) break;    
        }
    }
    is_done = true;
    Flush();
}

void MTSort::Flush()
{
    hm.Flush();
}

Sorters::Sorters(TEventQueue_t &input, OCL::UserConfiguration &config, const char *tree_name, const char *_user_sort)
    : input_queue( input )
    , histograms( )
    , sorters( )
    , user_config( config )
    , user_sort_path( ( _user_sort ) ? _user_sort : "" )
    , tree_file_name( ( tree_name ) ? tree_name : "" )
    , tree_files( {tree_file_name} )
{

}

Sorters::~Sorters()
{
    for ( auto &v : sorters ){
        delete v;
    }
}

void Sorters::flush()
{
    for ( auto &v : sorters ){
        v->Flush();
    }
}

MTSort *Sorters::GetNewSorter()
{
    sorters.push_back(new MTSort(input_queue, output_queue, histograms, user_config,
                                 (user_sort_path.empty()) ? nullptr : user_sort_path.c_str()));
    return sorters.back();
}

TreeWriter *Sorters::GetNewTreeWriter()
{
    return new TreeWriter(output_queue, tree_file_name.c_str(), this);
}