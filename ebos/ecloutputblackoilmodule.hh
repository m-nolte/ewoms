// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/
/*!
 * \file
 * \copydoc Ewoms::EclOutputBlackOilModule
 */
#ifndef EWOMS_ECL_OUTPUT_BLACK_OIL_MODULE_HH
#define EWOMS_ECL_OUTPUT_BLACK_OIL_MODULE_HH

#include "eclwriter.hh"

#include <ewoms/common/propertysystem.hh>
#include <ewoms/common/parametersystem.hh>

#include <opm/common/Valgrind.hpp>
#include <opm/parser/eclipse/Units/Units.hpp>
#include <opm/output/data/Cells.hpp>
#include <opm/output/eclipse/EclipseIO.hpp>


#include <dune/common/fvector.hh>

#include <type_traits>

namespace Ewoms {
namespace Properties {
// create new type tag for the Ecl-output
NEW_TYPE_TAG(EclOutputBlackOil);

} // namespace Properties

// forward declaration
template <class TypeTag>
class EcfvDiscretization;

/*!
 * \ingroup EclBlackOilSimulator
 *
 * \brief Output module for the results black oil model writing in
 *        ECL binary format.
 */
template <class TypeTag>
class EclOutputBlackOilModule
{

    typedef typename GET_PROP_TYPE(TypeTag, Simulator) Simulator;
    typedef typename GET_PROP_TYPE(TypeTag, Discretization) Discretization;
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, Evaluation) Evaluation;
    typedef typename GET_PROP_TYPE(TypeTag, ElementContext) ElementContext;
    typedef typename GET_PROP_TYPE(TypeTag, MaterialLaw) MaterialLaw;
    typedef typename GET_PROP_TYPE(TypeTag, MaterialLawParams) MaterialLawParams;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;

    enum { numPhases = FluidSystem::numPhases };
    enum { oilPhaseIdx = FluidSystem::oilPhaseIdx };
    enum { gasPhaseIdx = FluidSystem::gasPhaseIdx };
    enum { waterPhaseIdx = FluidSystem::waterPhaseIdx };
    enum { gasCompIdx = FluidSystem::gasCompIdx };
    enum { oilCompIdx = FluidSystem::oilCompIdx };

    typedef std::vector<Scalar> ScalarBuffer;

public:
    EclOutputBlackOilModule(const Simulator& simulator)
        : simulator_(simulator)
    { }

    /*!
     * \brief Allocate memory for the scalar fields we would like to
     *        write to ECL output files
     */
    void allocBuffers(unsigned bufferSize, unsigned reportStepNum, const Opm::RestartConfig& restartConfig, const bool substep, const bool log)
    {

        if (!std::is_same<Discretization, Ewoms::EcfvDiscretization<TypeTag> >::value)
            return;

        std::map<std::string, int> rstKeywords = restartConfig.getRestartKeywords(reportStepNum);
        for (auto& keyValue : rstKeywords) {
            keyValue.second = restartConfig.getKeyword(keyValue.first, reportStepNum);
        }

        for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++ phaseIdx) {
            if (!FluidSystem::phaseIsActive(phaseIdx))
                continue;

            saturation_[phaseIdx].resize(bufferSize,0.0);
        }

        oilPressure_.resize(bufferSize,0.0);
        temperature_.resize(bufferSize,0.0);

        // Output the same as legacy
        // TODO: Only needed if DISGAS or VAPOIL
        if (true)
            rs_.resize(bufferSize,0.0);
        if (true)
            rv_.resize(bufferSize,0.0);

        if (GET_PROP_VALUE(TypeTag, EnableSolvent)) {
            sSol_.resize(bufferSize,0.0);
        }
        if (GET_PROP_VALUE(TypeTag, EnablePolymer)) {
            cPolymer_.resize(bufferSize,0.0);
        }

        // Output the same as legacy
        // TODO: Only needed if Vappars or hysteresis.
        soMax_.resize(bufferSize,0.0);
        pcSwMdcOw_.resize(bufferSize,0.0);
        krnSwMdcOw_.resize(bufferSize,0.0);
        pcSwMdcGo_.resize(bufferSize,0.0);
        krnSwMdcGo_.resize(bufferSize,0.0);

        // Only provide RESTART_AUXILIARY if it is asked for by the user
        if (!restartConfig.getWriteRestartFile(reportStepNum) || substep)
            return;

        // Output the same as legacy
        // TODO: Only needed if DISGAS or VAPOIL
        if (FluidSystem::phaseIsActive(oilPhaseIdx) && FluidSystem::phaseIsActive(gasPhaseIdx)) {
            if (rstKeywords["RSSAT"] > 0) {
                rstKeywords["RSSAT"] = 0;
                gasDissolutionFactor_.resize(bufferSize,0.0);
            }
            if (rstKeywords["RVSAT"] > 0) {
                rstKeywords["RVSAT"] = 0;
                oilVaporizationFactor_.resize(bufferSize,0.0);
            }
        }

        if (FluidSystem::phaseIsActive(waterPhaseIdx) && rstKeywords["BW"] > 0)
        {
            rstKeywords["BW"] = 0;
            invB_[waterPhaseIdx].resize(bufferSize,0.0);
        }
        if (FluidSystem::phaseIsActive(oilPhaseIdx) && rstKeywords["BO"] > 0)
        {
            rstKeywords["BO"] = 0;
            invB_[oilPhaseIdx].resize(bufferSize,0.0);
        }
        if (FluidSystem::phaseIsActive(gasPhaseIdx) && rstKeywords["BG"] > 0)
        {
            rstKeywords["BG"] = 0;
            invB_[gasPhaseIdx].resize(bufferSize,0.0);
        }

        if (rstKeywords["DEN"] > 0) {
            rstKeywords["DEN"] = 0;
            for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++ phaseIdx) {
                if (!FluidSystem::phaseIsActive(phaseIdx))
                    continue;
                density_[phaseIdx].resize(bufferSize,0.0);
            }
        }
        const bool hasVWAT = (rstKeywords["VISC"] > 0) || (rstKeywords["VWAT"] > 0);
        const bool hasVOIL = (rstKeywords["VISC"] > 0) || (rstKeywords["VOIL"] > 0);
        const bool hasVGAS = (rstKeywords["VISC"] > 0) || (rstKeywords["VGAS"] > 0);
        rstKeywords["VISC"] = 0;

        if (FluidSystem::phaseIsActive(waterPhaseIdx) && hasVWAT)
        {
            rstKeywords["VWAT"] = 0;
            viscosity_[waterPhaseIdx].resize(bufferSize,0.0);
        }
        if (FluidSystem::phaseIsActive(oilPhaseIdx) && hasVOIL > 0)
        {
            rstKeywords["VOIL"] = 0;
            viscosity_[oilPhaseIdx].resize(bufferSize,0.0);
        }
        if (FluidSystem::phaseIsActive(gasPhaseIdx) && hasVGAS > 0)
        {
            rstKeywords["VGAS"] = 0;
            viscosity_[gasPhaseIdx].resize(bufferSize,0.0);
        }

        if (FluidSystem::phaseIsActive(waterPhaseIdx) && rstKeywords["KRW"] > 0)
        {
            rstKeywords["KRW"] = 0;
            relativePermeability_[waterPhaseIdx].resize(bufferSize,0.0);
        }
        if (FluidSystem::phaseIsActive(oilPhaseIdx) && rstKeywords["KRO"] > 0)
        {
            rstKeywords["KRO"] = 0;
            relativePermeability_[oilPhaseIdx].resize(bufferSize,0.0);
        }
        if (FluidSystem::phaseIsActive(gasPhaseIdx) && rstKeywords["KRG"] > 0)
        {
            rstKeywords["KRG"] = 0;
            relativePermeability_[gasPhaseIdx].resize(bufferSize,0.0);
        }

        if (rstKeywords["PBPD"] > 0)  {
            rstKeywords["PBPD"] = 0;
            bubblePointPressure_.resize(bufferSize,0.0);
            dewPointPressure_.resize(bufferSize,0.0);
        }

        //Warn for any unhandled keyword
        if (log) {
            for (auto& keyValue : rstKeywords) {
                if (keyValue.second > 0) {
                    std::string logstring = "Keyword '";
                    logstring.append(keyValue.first);
                    logstring.append("' is unhandled for output to file.");
                    Opm::OpmLog::warning("Unhandled output keyword", logstring);
                }
            }
        }

        failedCellsPb_.clear();
        failedCellsPd_.clear();

        // Not supported in flow legacy
        if (false)
            saturatedOilFormationVolumeFactor_.resize(bufferSize,0.0);
        if (false)
            oilSaturationPressure_.resize(bufferSize,0.0);
    }

    /*!
     * \brief Modify the internal buffers according to the intensive quanties relevant
     *        for an element
     */
    void processElement(const ElementContext& elemCtx)
    {
        typedef Opm::MathToolbox<Evaluation> Toolbox;

        if (!std::is_same<Discretization, Ewoms::EcfvDiscretization<TypeTag> >::value)
            return;

        for (unsigned dofIdx = 0; dofIdx < elemCtx.numPrimaryDof(/*timeIdx=*/0); ++dofIdx) {
            const auto& intQuants = elemCtx.intensiveQuantities(dofIdx, /*timeIdx=*/0);
            const auto& fs = intQuants.fluidState();
            typedef typename std::remove_const<typename std::remove_reference<decltype(fs)>::type>::type FluidState;
            unsigned globalDofIdx = elemCtx.globalSpaceIndex(dofIdx, /*timeIdx=*/0);
            unsigned pvtRegionIdx = elemCtx.primaryVars(dofIdx, /*timeIdx=*/0).pvtRegionIndex();

            for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++ phaseIdx) {
                if (saturation_[phaseIdx].size() == 0)
                    continue;

                saturation_[phaseIdx][globalDofIdx] = Toolbox::value(fs.saturation(phaseIdx));
                Opm::Valgrind::CheckDefined(saturation_[phaseIdx][globalDofIdx]);
            }

            if (oilPressure_.size() > 0) {
                oilPressure_[globalDofIdx] = Toolbox::value(fs.pressure(oilPhaseIdx));
                Opm::Valgrind::CheckDefined(oilPressure_[globalDofIdx]);

            }
            if (gasDissolutionFactor_.size() > 0) {
                Scalar SoMax = elemCtx.problem().maxOilSaturation(globalDofIdx);
                gasDissolutionFactor_[globalDofIdx] =
                    FluidSystem::template saturatedDissolutionFactor<FluidState, Scalar>(fs, oilPhaseIdx, pvtRegionIdx, SoMax);
                Opm::Valgrind::CheckDefined(gasDissolutionFactor_[globalDofIdx]);

            }
            if (oilVaporizationFactor_.size() > 0) {
                Scalar SoMax = elemCtx.problem().maxOilSaturation(globalDofIdx);
                oilVaporizationFactor_[globalDofIdx] =
                    FluidSystem::template saturatedDissolutionFactor<FluidState, Scalar>(fs, gasPhaseIdx, pvtRegionIdx, SoMax);
                Opm::Valgrind::CheckDefined(oilVaporizationFactor_[globalDofIdx]);

            }
            if (gasFormationVolumeFactor_.size() > 0) {
                gasFormationVolumeFactor_[globalDofIdx] =
                    1.0/FluidSystem::template inverseFormationVolumeFactor<FluidState, Scalar>(fs, gasPhaseIdx, pvtRegionIdx);
                Opm::Valgrind::CheckDefined(gasFormationVolumeFactor_[globalDofIdx]);

            }
            if (saturatedOilFormationVolumeFactor_.size() > 0) {
                saturatedOilFormationVolumeFactor_[globalDofIdx] =
                    1.0/FluidSystem::template saturatedInverseFormationVolumeFactor<FluidState, Scalar>(fs, oilPhaseIdx, pvtRegionIdx);
                Opm::Valgrind::CheckDefined(saturatedOilFormationVolumeFactor_[globalDofIdx]);

            }
            if (oilSaturationPressure_.size() > 0) {
                oilSaturationPressure_[globalDofIdx] =
                    FluidSystem::template saturationPressure<FluidState, Scalar>(fs, oilPhaseIdx, pvtRegionIdx);
                Opm::Valgrind::CheckDefined(oilSaturationPressure_[globalDofIdx]);

            }

            if (rs_.size()) {
                rs_[globalDofIdx] = Toolbox::value(fs.Rs());
                Opm::Valgrind::CheckDefined(rs_[globalDofIdx]);
            }

            if (rv_.size()) {
                rv_[globalDofIdx] = Toolbox::value(fs.Rv());
                Opm::Valgrind::CheckDefined(rv_[globalDofIdx]);
            }

            for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++ phaseIdx) {
                if (invB_[phaseIdx].size() == 0)
                    continue;

                invB_[phaseIdx][globalDofIdx] = Toolbox::value(fs.invB(phaseIdx));
                Opm::Valgrind::CheckDefined(invB_[phaseIdx][globalDofIdx]);
            }

            for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++ phaseIdx) {
                if (density_[phaseIdx].size() == 0)
                    continue;

                density_[phaseIdx][globalDofIdx] = Toolbox::value(fs.density(phaseIdx));
                Opm::Valgrind::CheckDefined(density_[phaseIdx][globalDofIdx]);
            }

            for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++ phaseIdx) {
                if (viscosity_[phaseIdx].size() == 0)
                    continue;

                viscosity_[phaseIdx][globalDofIdx] = Toolbox::value(fs.viscosity(phaseIdx));
                Opm::Valgrind::CheckDefined(viscosity_[phaseIdx][globalDofIdx]);
            }

            for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++ phaseIdx) {
                if (relativePermeability_[phaseIdx].size() == 0)
                    continue;

                relativePermeability_[phaseIdx][globalDofIdx] = Toolbox::value(intQuants.relativePermeability(phaseIdx));
                Opm::Valgrind::CheckDefined(relativePermeability_[phaseIdx][globalDofIdx]);
            }

            if (sSol_.size() > 0) {
                sSol_[globalDofIdx] = intQuants.solventSaturation().value();
            }

            if (cPolymer_.size() > 0) {
                cPolymer_[globalDofIdx] = intQuants.polymerConcentration().value();
            }

            if (bubblePointPressure_.size() > 0)
            {
                try {
                    bubblePointPressure_[globalDofIdx] = Toolbox::value(FluidSystem::bubblePointPressure(fs, intQuants.pvtRegionIndex()));
                }
                catch (const Opm::NumericalProblem& e) {
                    const auto globalIdx = elemCtx.simulator().gridManager().grid().globalCell()[globalDofIdx];
                    failedCellsPb_.push_back(globalIdx);
                }
            }
            if (dewPointPressure_.size() > 0)
            {
                try {
                    dewPointPressure_[globalDofIdx] = Toolbox::value(FluidSystem::dewPointPressure(fs, intQuants.pvtRegionIndex()));
                }
                catch (const Opm::NumericalProblem& e) {
                    const auto globalIdx = elemCtx.simulator().gridManager().grid().globalCell()[globalDofIdx];
                    failedCellsPd_.push_back(globalIdx);
                }
            }

            if (soMax_.size() > 0)
                soMax_[globalDofIdx] = elemCtx.simulator().problem().maxOilSaturation(globalDofIdx);

            const auto& matLawManager = elemCtx.simulator().problem().materialLawManager();
            if (matLawManager->enableHysteresis()) {
                if (pcSwMdcOw_.size() > 0 && krnSwMdcOw_.size() > 0) {
                    matLawManager->oilWaterHysteresisParams(
                                pcSwMdcOw_[globalDofIdx],
                                krnSwMdcOw_[globalDofIdx],
                                globalDofIdx);
                }
                if (pcSwMdcGo_.size() > 0 && krnSwMdcGo_.size() > 0) {
                    matLawManager->gasOilHysteresisParams(
                                pcSwMdcGo_[globalDofIdx],
                                krnSwMdcGo_[globalDofIdx],
                                globalDofIdx);
                }
            }

            // hack to make the intial output of rs and rv Ecl compatible.
            // For cells with swat == 1 Ecl outputs; rs = rsSat and rv=rvSat, in all but the initial step
            // where it outputs rs and rv values calculated by the initialization. To be compatible we overwrite
            // rs and rv with the values computed in the initially.
            // Volume factors, densities and viscosities need to be recalculated with the updated rs and rv values.
            // This can be removed when ebos has 100% controll over output
            if (elemCtx.simulator().episodeIndex() < 0 && FluidSystem::phaseIsActive(oilPhaseIdx) && FluidSystem::phaseIsActive(gasPhaseIdx) ) {

                const auto& fs_initial = elemCtx.simulator().problem().initialFluidState(globalDofIdx);

                // use initial rs and rv values
                if (rv_.size() > 0)
                    rv_[globalDofIdx] = fs_initial.Rv();

                if (rs_.size() > 0)
                    rs_[globalDofIdx] = fs_initial.Rs();

                // re-compute the volume factors, viscosities and densities if asked for
                if (density_[oilPhaseIdx].size() > 0)
                    density_[oilPhaseIdx][globalDofIdx] = FluidSystem::density(fs_initial,
                                                                                    oilPhaseIdx,
                                                                                    intQuants.pvtRegionIndex());
                if (density_[gasPhaseIdx].size() > 0)
                    density_[gasPhaseIdx][globalDofIdx] = FluidSystem::density(fs_initial,
                                                                                    gasPhaseIdx,
                                                                                    intQuants.pvtRegionIndex());

                if (invB_[oilPhaseIdx].size() > 0)
                    invB_[oilPhaseIdx][globalDofIdx] = FluidSystem::inverseFormationVolumeFactor(fs_initial,
                                                                                                      oilPhaseIdx,
                                                                                                      intQuants.pvtRegionIndex());
                if (invB_[gasPhaseIdx].size() > 0)
                    invB_[gasPhaseIdx][globalDofIdx] = FluidSystem::inverseFormationVolumeFactor(fs_initial,
                                                                                                      gasPhaseIdx,
                                                                                                      intQuants.pvtRegionIndex());
                if (viscosity_[oilPhaseIdx].size() > 0)
                    viscosity_[oilPhaseIdx][globalDofIdx] = FluidSystem::viscosity(fs_initial,
                                                                                        oilPhaseIdx,
                                                                                        intQuants.pvtRegionIndex());
                if (viscosity_[gasPhaseIdx].size() > 0)
                    viscosity_[gasPhaseIdx][globalDofIdx] = FluidSystem::viscosity(fs_initial,
                                                                                        gasPhaseIdx,
                                                                                        intQuants.pvtRegionIndex());
            }

        }

    }


    void outputErrorLog()
    {
        const size_t maxNumCellsFaillog = 20;

        int pbSize = failedCellsPb_.size(), pd_size = failedCellsPd_.size();
        std::vector<int> displPb, displPd, recvLenPb, recvLenPd;
        const auto& comm = simulator_.gridView().comm();

        if ( isIORank_() )
        {
            displPb.resize(comm.size()+1, 0);
            displPd.resize(comm.size()+1, 0);
            recvLenPb.resize(comm.size());
            recvLenPd.resize(comm.size());
        }

        comm.gather(&pbSize, recvLenPb.data(), 1, 0);
        comm.gather(&pd_size, recvLenPd.data(), 1, 0);
        std::partial_sum(recvLenPb.begin(), recvLenPb.end(), displPb.begin()+1);
        std::partial_sum(recvLenPd.begin(), recvLenPd.end(), displPd.begin()+1);
        std::vector<int> globalFailedCellsPb, globalFailedCellsPd;

        if ( isIORank_() )
        {
            globalFailedCellsPb.resize(displPb.back());
            globalFailedCellsPd.resize(displPd.back());
        }

        comm.gatherv(failedCellsPb_.data(), static_cast<int>(failedCellsPb_.size()),
                     globalFailedCellsPb.data(), recvLenPb.data(),
                     displPb.data(), 0);
        comm.gatherv(failedCellsPd_.data(), static_cast<int>(failedCellsPd_.size()),
                     globalFailedCellsPd.data(),  recvLenPd.data(),
                     displPd.data(), 0);
        std::sort(globalFailedCellsPb.begin(), globalFailedCellsPb.end());
        std::sort(globalFailedCellsPd.begin(), globalFailedCellsPd.end());

        if (globalFailedCellsPb.size() > 0) {
            std::stringstream errlog;
            errlog << "Finding the bubble point pressure failed for " << globalFailedCellsPb.size() << " cells [";
            errlog << globalFailedCellsPb[0];
            const size_t max_elems = std::min(maxNumCellsFaillog, globalFailedCellsPb.size());
            for (size_t i = 1; i < max_elems; ++i) {
                errlog << ", " << globalFailedCellsPb[i];
            }
            if (globalFailedCellsPb.size() > maxNumCellsFaillog) {
                errlog << ", ...";
            }
            errlog << "]";
            Opm::OpmLog::warning("Bubble point numerical problem", errlog.str());
        }
        if (globalFailedCellsPd.size() > 0) {
            std::stringstream errlog;
            errlog << "Finding the dew point pressure failed for " << globalFailedCellsPd.size() << " cells [";
            errlog << globalFailedCellsPd[0];
            const size_t max_elems = std::min(maxNumCellsFaillog, globalFailedCellsPd.size());
            for (size_t i = 1; i < max_elems; ++i) {
                errlog << ", " << globalFailedCellsPd[i];
            }
            if (globalFailedCellsPd.size() > maxNumCellsFaillog) {
                errlog << ", ...";
            }
            errlog << "]";
            Opm::OpmLog::warning("Dew point numerical problem", errlog.str());
        }
    }

    /*!
     * \brief Move all buffers to data::Solution.
     */
    void assignToSolution(Opm::data::Solution& sol)
    {
        if (!std::is_same<Discretization, Ewoms::EcfvDiscretization<TypeTag> >::value)
            return;

        if ( oilPressure_.size() > 0 ) {
            sol.insert( "PRESSURE", Opm::UnitSystem::measure::pressure, std::move(oilPressure_), Opm::data::TargetType::RESTART_SOLUTION);
        }

        if ( temperature_.size() > 0 ) {
            sol.insert( "TEMP", Opm::UnitSystem::measure::temperature, std::move(temperature_), Opm::data::TargetType::RESTART_SOLUTION);
        }

        if( FluidSystem::phaseIsActive(waterPhaseIdx) && saturation_[waterPhaseIdx].size() > 0 ) {
            sol.insert( "SWAT", Opm::UnitSystem::measure::identity, std::move(saturation_[waterPhaseIdx]), Opm::data::TargetType::RESTART_SOLUTION );
        }
        if( FluidSystem::phaseIsActive(gasPhaseIdx) && saturation_[gasPhaseIdx].size() > 0) {
            sol.insert( "SGAS", Opm::UnitSystem::measure::identity, std::move(saturation_[gasPhaseIdx]), Opm::data::TargetType::RESTART_SOLUTION );
        }

        if ( gasDissolutionFactor_.size() > 0 ) {
            sol.insert( "RSSAT", Opm::UnitSystem::measure::gas_oil_ratio, std::move(gasDissolutionFactor_), Opm::data::TargetType::RESTART_AUXILIARY );

        }
        if ( oilVaporizationFactor_.size() > 0 ) {
            sol.insert( "RVSAT", Opm::UnitSystem::measure::oil_gas_ratio, std::move(oilVaporizationFactor_) , Opm::data::TargetType::RESTART_AUXILIARY );
        }
        if ( rs_.size() > 0 ) {
            sol.insert( "RS", Opm::UnitSystem::measure::gas_oil_ratio, std::move(rs_), Opm::data::TargetType::RESTART_SOLUTION );

        }
        if (rv_.size() > 0 ) {
            sol.insert( "RV", Opm::UnitSystem::measure::oil_gas_ratio, std::move(rv_) , Opm::data::TargetType::RESTART_SOLUTION );
        }
        if (invB_[waterPhaseIdx].size() > 0 ) {
            sol.insert( "1OVERBW", Opm::UnitSystem::measure::water_inverse_formation_volume_factor, std::move(invB_[waterPhaseIdx]), Opm::data::TargetType::RESTART_AUXILIARY );
        }
        if (invB_[oilPhaseIdx].size() > 0 ) {
            sol.insert( "1OVERBO", Opm::UnitSystem::measure::oil_inverse_formation_volume_factor, std::move(invB_[oilPhaseIdx]), Opm::data::TargetType::RESTART_AUXILIARY );
        }
        if (invB_[gasPhaseIdx].size() > 0 ) {
            sol.insert( "1OVERBG", Opm::UnitSystem::measure::gas_inverse_formation_volume_factor, std::move(invB_[gasPhaseIdx]), Opm::data::TargetType::RESTART_AUXILIARY );
        }

        if (density_[waterPhaseIdx].size() > 0 ) {
            sol.insert( "WAT_DEN", Opm::UnitSystem::measure::density, std::move(density_[waterPhaseIdx]), Opm::data::TargetType::RESTART_AUXILIARY );
        }
        if (density_[oilPhaseIdx].size() > 0 ) {
            sol.insert( "OIL_DEN", Opm::UnitSystem::measure::density, std::move(density_[oilPhaseIdx]), Opm::data::TargetType::RESTART_AUXILIARY );
        }
        if (density_[gasPhaseIdx].size() > 0 ) {
            sol.insert( "GAS_DEN", Opm::UnitSystem::measure::density, std::move(density_[gasPhaseIdx]), Opm::data::TargetType::RESTART_AUXILIARY );
        }

        if (viscosity_[waterPhaseIdx].size() > 0 ) {
            sol.insert( "WAT_VISC", Opm::UnitSystem::measure::viscosity, std::move(viscosity_[waterPhaseIdx]), Opm::data::TargetType::RESTART_AUXILIARY);
        }
        if (viscosity_[oilPhaseIdx].size() > 0 ) {
            sol.insert( "OIL_VISC", Opm::UnitSystem::measure::viscosity, std::move(viscosity_[oilPhaseIdx]), Opm::data::TargetType::RESTART_AUXILIARY);
        }
        if (viscosity_[gasPhaseIdx].size() > 0 ) {
            sol.insert( "GAS_VISC", Opm::UnitSystem::measure::viscosity, std::move(viscosity_[gasPhaseIdx]), Opm::data::TargetType::RESTART_AUXILIARY );
        }

        if (relativePermeability_[waterPhaseIdx].size() > 0) {
            sol.insert( "WATKR", Opm::UnitSystem::measure::identity, std::move(relativePermeability_[waterPhaseIdx]), Opm::data::TargetType::RESTART_AUXILIARY);
        }
        if (relativePermeability_[oilPhaseIdx].size() > 0 ) {
            sol.insert( "OILKR", Opm::UnitSystem::measure::identity, std::move(relativePermeability_[oilPhaseIdx]), Opm::data::TargetType::RESTART_AUXILIARY);
        }
        if (relativePermeability_[gasPhaseIdx].size() > 0 ) {
            sol.insert( "GASKR", Opm::UnitSystem::measure::identity, std::move(relativePermeability_[gasPhaseIdx]), Opm::data::TargetType::RESTART_AUXILIARY);
        }

        if (pcSwMdcOw_.size() > 0 )
            sol.insert ("PCSWM_OW", Opm::UnitSystem::measure::identity, std::move(pcSwMdcOw_), Opm::data::TargetType::RESTART_AUXILIARY);

        if (krnSwMdcOw_.size() > 0)
            sol.insert ("KRNSW_OW", Opm::UnitSystem::measure::identity, std::move(krnSwMdcOw_), Opm::data::TargetType::RESTART_AUXILIARY);

        if (pcSwMdcGo_.size() > 0)
            sol.insert ("PCSWM_GO", Opm::UnitSystem::measure::identity, std::move(pcSwMdcGo_), Opm::data::TargetType::RESTART_AUXILIARY);

        if (krnSwMdcGo_.size() > 0)
            sol.insert ("KRNSW_GO", Opm::UnitSystem::measure::identity, std::move(krnSwMdcGo_), Opm::data::TargetType::RESTART_AUXILIARY);

        if (soMax_.size() > 0)
            sol.insert ("SOMAX", Opm::UnitSystem::measure::identity, std::move(soMax_), Opm::data::TargetType::RESTART_SOLUTION);

        if (sSol_.size() > 0)
            sol.insert ("SSOL", Opm::UnitSystem::measure::identity, std::move(sSol_), Opm::data::TargetType::RESTART_SOLUTION);

        if (cPolymer_.size() > 0)
            sol.insert ("POLYMER", Opm::UnitSystem::measure::identity, std::move(cPolymer_), Opm::data::TargetType::RESTART_SOLUTION);

        if (dewPointPressure_.size() > 0)
            sol.insert ("PDEW", Opm::UnitSystem::measure::pressure, std::move(dewPointPressure_), Opm::data::TargetType::RESTART_AUXILIARY);

        if (bubblePointPressure_.size() > 0)
            sol.insert ("PBUB", Opm::UnitSystem::measure::pressure, std::move(bubblePointPressure_), Opm::data::TargetType::RESTART_AUXILIARY);

}

    void setRestart(const Opm::data::Solution& sol, unsigned elemIdx, unsigned globalDofIndex) 
    {

        Scalar so = 1.0;
        if( sol.has( "SWAT" ) ) {
            saturation_[waterPhaseIdx][elemIdx] = sol.data("SWAT")[globalDofIndex];
            so -= sol.data("SWAT")[globalDofIndex];
        }

        if( sol.has( "SGAS" ) ) {
            saturation_[gasPhaseIdx][elemIdx] = sol.data("SGAS")[globalDofIndex];
            so -= sol.data("SGAS")[globalDofIndex];
        }

        saturation_[oilPhaseIdx][elemIdx] = so;

        if( sol.has( "PRESSURE" ) ) {
            oilPressure_[elemIdx] = sol.data( "PRESSURE" )[globalDofIndex];
        }

        if( sol.has( "TEMP" ) ) {
            temperature_[elemIdx] = sol.data( "TEMP" )[globalDofIndex];
        }

        if( sol.has( "RS" ) ) {
            rs_[elemIdx] = sol.data("RS")[globalDofIndex];
        }

        if( sol.has( "RV" ) ) {
            rv_[elemIdx] = sol.data("RV")[globalDofIndex];
        }

        if ( sol.has( "SSOL" ) ) {
            sSol_[elemIdx] = sol.data("SSOL")[globalDofIndex];
        }

        if ( sol.has("POLYMER" ) ) {
            cPolymer_[elemIdx] = sol.data("POLYMER")[globalDofIndex];
        }

        if ( sol.has("SOMAX" ) ) {
            soMax_[elemIdx] = sol.data("SOMAX")[globalDofIndex];
        }

        if ( sol.has("PCSWM_OW" ) ) {
            pcSwMdcOw_[elemIdx] = sol.data("PCSWM_OW")[globalDofIndex];
        }

        if ( sol.has("KRNSW_OW" ) ) {
            krnSwMdcOw_[elemIdx] = sol.data("KRNSW_OW")[globalDofIndex];
        }

        if ( sol.has("PCSWM_GO" ) ) {
            pcSwMdcGo_[elemIdx] = sol.data("PCSWM_GO")[globalDofIndex];
        }

        if ( sol.has("KRNSW_GO" ) ) {
            krnSwMdcGo_[elemIdx] = sol.data("KRNSW_GO")[globalDofIndex];
        }
    }


    template <class FluidState>
    void assignToFluidState(FluidState& fs, unsigned elemIdx) const 
    {

        for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++ phaseIdx) {
            if (saturation_[phaseIdx].size() == 0)
                continue;

            fs.setSaturation(phaseIdx, saturation_[phaseIdx][elemIdx]);
        }

        if (oilPressure_.size() > 0) {

            // this assumes that capillary pressures only depend on the phase saturations
            // and possibly on temperature. (this is always the case for ECL problems.)
            Dune::FieldVector< Scalar, numPhases > pc( 0 );
            const MaterialLawParams& matParams = simulator_.problem().materialLawParams(elemIdx);
            MaterialLaw::capillaryPressures(pc, matParams, fs);
            Opm::Valgrind::CheckDefined(oilPressure_[elemIdx]);
            Opm::Valgrind::CheckDefined(pc);
            assert(FluidSystem::phaseIsActive(oilPhaseIdx));
            for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++phaseIdx) {
                if (!FluidSystem::phaseIsActive(phaseIdx))
                    continue;

                fs.setPressure(phaseIdx, oilPressure_[elemIdx] + (pc[phaseIdx] - pc[oilPhaseIdx]));
            }
        }

        if (temperature_.size() > 0) {
            fs.setTemperature( temperature_[elemIdx]);
        }

        if (rs_.size() > 0) {
           fs.setRs(rs_[elemIdx]);

        }
        if (rv_.size() > 0) {
           fs.setRv(rv_[elemIdx]);
        }
    }

    void initHysteresisParams(Simulator& simulator, unsigned elemIdx) const {

        if (soMax_.size() > 0)
            simulator.problem().setMaxOilSaturation(soMax_[elemIdx], elemIdx);

        if (simulator.problem().materialLawManager()->enableHysteresis()) {
            auto matLawManager = simulator.problem().materialLawManager();

            if (pcSwMdcOw_.size() > 0 && krnSwMdcOw_.size() > 0) {
                matLawManager->setOilWaterHysteresisParams(
                            pcSwMdcOw_[elemIdx],
                            krnSwMdcOw_[elemIdx],
                            elemIdx);
            }
            if (pcSwMdcGo_.size() > 0 && krnSwMdcGo_.size() > 0) {
                matLawManager->setGasOilHysteresisParams(
                            pcSwMdcGo_[elemIdx],
                            krnSwMdcGo_[elemIdx],
                            elemIdx);
            }
        }

    }

    Scalar getSolventSaturation(unsigned elemIdx) const {
        if(sSol_.size() > 0)
            return sSol_[elemIdx];

        return 0;
    }

    Scalar getPolymerConcentration(unsigned elemIdx) const {
        if(cPolymer_.size() > 0)
            return cPolymer_[elemIdx];

        return 0;
    }


private:

    bool isIORank_() const
    {
        const auto& comm = simulator_.gridView().comm();
        return comm.rank() == 0;
    }

    const Simulator& simulator_;

    ScalarBuffer saturation_[numPhases];
    ScalarBuffer oilPressure_;
    ScalarBuffer temperature_;
    ScalarBuffer gasDissolutionFactor_;
    ScalarBuffer oilVaporizationFactor_;
    ScalarBuffer gasFormationVolumeFactor_;
    ScalarBuffer saturatedOilFormationVolumeFactor_;
    ScalarBuffer oilSaturationPressure_;
    ScalarBuffer rs_;
    ScalarBuffer rv_;
    ScalarBuffer invB_[numPhases];
    ScalarBuffer density_[numPhases];
    ScalarBuffer viscosity_[numPhases];
    ScalarBuffer relativePermeability_[numPhases];
    ScalarBuffer sSol_;
    ScalarBuffer cPolymer_;
    ScalarBuffer soMax_;
    ScalarBuffer pcSwMdcOw_;
    ScalarBuffer krnSwMdcOw_;
    ScalarBuffer pcSwMdcGo_;
    ScalarBuffer krnSwMdcGo_;
    ScalarBuffer bubblePointPressure_;
    ScalarBuffer dewPointPressure_;
    std::vector<int> failedCellsPb_;
    std::vector<int> failedCellsPd_;

};
} // namespace Ewoms

#endif
