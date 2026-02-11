//
// Created by Vetle Wegner Ingeberg on 19/04/2023.
//

#ifndef DETECTORTYPES_H
#define DETECTORTYPES_H

#define NUM_LABR_DETECTORS 12   //!< Number of LaBr detectors
#define NUM_QINT_DETECTORS 1

enum DetectorType {
    invalid,    //!< Invalid address
    labr,       //!< Is a labr detector
    qint,
    any,        //!< Any detector
    unused      //!< Is a unused XIA channel
};


#endif // DETECTORTYPES_H
