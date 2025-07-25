#include "power1modulemeasprogram.h"
#include "power1module.h"
#include "power1moduleconfiguration.h"
#include "power1dspmodefunctioncatalog.h"
#include "veinvalidatorphasestringgenerator.h"
#include "measmodephasepersistency.h"
#include <errormessages.h>
#include <reply.h>
#include <proxy.h>
#include <doublevalidator.h>
#include <intvalidator.h>
#include <inttohexstringconvert.h>
#include <math.h>
#include <scpi.h>

namespace POWER1MODULE
{

cPower1ModuleMeasProgram::cPower1ModuleMeasProgram(cPower1Module* module, std::shared_ptr<BaseModuleConfiguration> pConfiguration) :
    cBaseDspMeasProgram(pConfiguration, module->getVeinModuleName()),
    m_pModule(module)
{
    m_dspInterface = m_pModule->getServiceInterfaceFactory()->createDspInterfacePower1(
        m_pModule->getEntityId(),
        &m_measModeSelector);

    m_IdentifyState.addTransition(this, &cModuleActivist::activationContinue, &m_claimResourcesSourceState);

    m_claimResourcesSourceState.addTransition(this, &cModuleActivist::activationContinue, &m_claimResourceSourceState);
    m_claimResourcesSourceState.addTransition(this, &cPower1ModuleMeasProgram::activationSkip, &m_pcbserverConnectState4measChannels);
    m_claimResourceSourceState.addTransition(this, &cModuleActivist::activationContinue, &m_claimResourceSourceDoneState);
    m_claimResourceSourceDoneState.addTransition(this, &cModuleActivist::activationContinue, &m_pcbserverConnectState4measChannels);
    m_claimResourceSourceDoneState.addTransition(this, &cModuleActivist::activationLoop, &m_claimResourceSourceState);

    m_pcbserverConnectState4measChannels.addTransition(this, &cModuleActivist::activationContinue, &m_pcbserverConnectState4freqChannels);
    m_pcbserverConnectState4freqChannels.addTransition(this, &cModuleActivist::activationContinue, &m_readSourceChannelInformationState);

    m_readSourceChannelInformationState.addTransition(this, &cModuleActivist::activationContinue, &m_readSourceChannelAliasState);
    m_readSourceChannelInformationState.addTransition(this, &cPower1ModuleMeasProgram::activationSkip, &m_dspserverConnectState);
    m_readSourceChannelAliasState.addTransition(this, &cModuleActivist::activationContinue, &m_readSourceDspChannelState);
    m_readSourceDspChannelState.addTransition(this, &cModuleActivist::activationContinue, &m_readSourceFormFactorState);
    m_readSourceFormFactorState.addTransition(this, &cModuleActivist::activationContinue, &m_readSourceChannelInformationDoneState);
    m_readSourceChannelInformationDoneState.addTransition(this, &cModuleActivist::activationLoop, &m_readSourceChannelAliasState);
    m_readSourceChannelInformationDoneState.addTransition(this, &cModuleActivist::activationContinue, &m_setSenseChannelRangeNotifiersState);

    m_setSenseChannelRangeNotifiersState.addTransition(this, &cModuleActivist::activationContinue, &m_setSenseChannelRangeNotifierState);
    m_setSenseChannelRangeNotifierState.addTransition(this, &cModuleActivist::activationContinue, &m_setSenseChannelRangeNotifierDoneState);
    m_setSenseChannelRangeNotifierDoneState.addTransition(this, &cModuleActivist::activationContinue, &m_dspserverConnectState);
    m_setSenseChannelRangeNotifierDoneState.addTransition(this, &cModuleActivist::activationLoop, &m_setSenseChannelRangeNotifierState);

    m_claimPGRMemState.addTransition(this, &cModuleActivist::activationContinue, &m_claimUSERMemState);
    m_claimUSERMemState.addTransition(this, &cModuleActivist::activationContinue, &m_var2DSPState);
    m_var2DSPState.addTransition(this, &cModuleActivist::activationContinue, &m_cmd2DSPState);
    m_cmd2DSPState.addTransition(this, &cModuleActivist::activationContinue, &m_activateDSPState);
    m_activateDSPState.addTransition(this, &cModuleActivist::activationContinue, &m_loadDSPDoneState);

    m_activationMachine.addState(&m_resourceManagerConnectState);
    m_activationMachine.addState(&m_IdentifyState);

    m_activationMachine.addState(&m_claimResourcesSourceState);
    m_activationMachine.addState(&m_claimResourceSourceState);
    m_activationMachine.addState(&m_claimResourceSourceDoneState);
    m_activationMachine.addState(&m_pcbserverConnectState4measChannels);
    m_activationMachine.addState(&m_pcbserverConnectState4freqChannels);

    m_activationMachine.addState(&m_readSourceChannelInformationState);
    m_activationMachine.addState(&m_readSourceChannelAliasState);
    m_activationMachine.addState(&m_readSourceDspChannelState);
    m_activationMachine.addState(&m_readSourceFormFactorState);
    m_activationMachine.addState(&m_readSourceChannelInformationDoneState);

    m_activationMachine.addState(&m_setSenseChannelRangeNotifiersState);
    m_activationMachine.addState(&m_setSenseChannelRangeNotifierState);
    m_activationMachine.addState(&m_setSenseChannelRangeNotifierDoneState);

    m_activationMachine.addState(&m_dspserverConnectState);
    m_activationMachine.addState(&m_claimPGRMemState);
    m_activationMachine.addState(&m_claimUSERMemState);
    m_activationMachine.addState(&m_var2DSPState);
    m_activationMachine.addState(&m_cmd2DSPState);
    m_activationMachine.addState(&m_activateDSPState);
    m_activationMachine.addState(&m_loadDSPDoneState);

    m_activationMachine.setInitialState(&m_resourceManagerConnectState);

    connect(&m_resourceManagerConnectState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::resourceManagerConnect);
    connect(&m_IdentifyState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::sendRMIdent);

    connect(&m_dspserverConnectState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::dspserverConnect);
    connect(&m_claimResourcesSourceState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::claimResourcesSource);
    connect(&m_claimResourceSourceState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::claimResourceSource);
    connect(&m_claimResourceSourceDoneState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::claimResourceSourceDone);

    connect(&m_pcbserverConnectState4measChannels, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::pcbserverConnect4measChannels);
    connect(&m_pcbserverConnectState4freqChannels, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::pcbserverConnect4freqChannels);

    connect(&m_readSourceChannelInformationState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::readSourceChannelInformation);
    connect(&m_readSourceChannelAliasState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::readSourceChannelAlias);
    connect(&m_readSourceDspChannelState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::readSourceDspChannel);
    connect(&m_readSourceFormFactorState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::readSourceFormFactor);
    connect(&m_readSourceChannelInformationDoneState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::readSourceChannelInformationDone);

    connect(&m_setSenseChannelRangeNotifiersState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::setSenseChannelRangeNotifiers);
    connect(&m_setSenseChannelRangeNotifierState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::setSenseChannelRangeNotifier);
    connect(&m_setSenseChannelRangeNotifierDoneState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::setSenseChannelRangeNotifierDone);

    connect(&m_claimPGRMemState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::claimPGRMem);
    connect(&m_claimUSERMemState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::claimUSERMem);
    connect(&m_var2DSPState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::varList2DSP);
    connect(&m_cmd2DSPState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::cmdList2DSP);
    connect(&m_activateDSPState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::activateDSP);
    connect(&m_loadDSPDoneState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::activateDSPdone);

    // setting up statemachine for unloading dsp and setting resources free
    m_freePGRMemState.addTransition(this, &cModuleActivist::deactivationContinue, &m_freeUSERMemState);
    m_freeUSERMemState.addTransition(this, &cModuleActivist::deactivationContinue, &m_freeFreqOutputsState);

    m_freeFreqOutputsState.addTransition(this, &cModuleActivist::deactivationContinue, &m_freeFreqOutputState);
    m_freeFreqOutputsState.addTransition(this, &cPower1ModuleMeasProgram::deactivationSkip, &m_resetNotifiersState);
    m_freeFreqOutputState.addTransition(this, &cModuleActivist::deactivationContinue, &m_freeFreqOutputDoneState);
    m_freeFreqOutputDoneState.addTransition(this, &cModuleActivist::deactivationContinue, &m_resetNotifiersState);
    m_freeFreqOutputDoneState.addTransition(this, &cModuleActivist::deactivationLoop, &m_freeFreqOutputState);
    m_resetNotifiersState.addTransition(this, &cModuleActivist::deactivationContinue, &m_resetNotifierState);
    m_resetNotifierState.addTransition(this, &cModuleActivist::deactivationContinue, &m_resetNotifierDoneState);
    m_resetNotifierDoneState.addTransition(this, &cModuleActivist::deactivationContinue, &m_unloadDSPDoneState);
    m_resetNotifierDoneState.addTransition(this, &cModuleActivist::deactivationLoop, &m_resetNotifierState);

    m_deactivationMachine.addState(&m_freePGRMemState);
    m_deactivationMachine.addState(&m_freeUSERMemState);

    m_deactivationMachine.addState(&m_freeFreqOutputsState);
    m_deactivationMachine.addState(&m_freeFreqOutputState);
    m_deactivationMachine.addState(&m_freeFreqOutputDoneState);

    m_deactivationMachine.addState(&m_resetNotifiersState);
    m_deactivationMachine.addState(&m_resetNotifierState);
    m_deactivationMachine.addState(&m_resetNotifierDoneState);

    m_deactivationMachine.addState(&m_unloadDSPDoneState);

    m_deactivationMachine.setInitialState(&m_freePGRMemState);

    connect(&m_freePGRMemState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::freePGRMem);
    connect(&m_freeUSERMemState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::freeUSERMem);

    connect(&m_freeFreqOutputsState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::freeFreqOutputs);
    connect(&m_freeFreqOutputState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::freeFreqOutput);
    connect(&m_freeFreqOutputDoneState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::freeFreqOutputDone);
    connect(&m_resetNotifiersState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::resetNotifiers);
    connect(&m_resetNotifierState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::resetNotifier);
    connect(&m_resetNotifierDoneState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::resetNotifierDone);

    connect(&m_unloadDSPDoneState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::deactivateDSPdone);

    // setting up statemachine for data acquisition
    m_dataAcquisitionState.addTransition(this, &cPower1ModuleMeasProgram::dataAquisitionContinue, &m_dataAcquisitionDoneState);
    m_dataAcquisitionMachine.addState(&m_dataAcquisitionState);
    m_dataAcquisitionMachine.addState(&m_dataAcquisitionDoneState);
    m_dataAcquisitionMachine.setInitialState(&m_dataAcquisitionState);
    connect(&m_dataAcquisitionState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::dataAcquisitionDSP);
    connect(&m_dataAcquisitionDoneState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::dataReadDSP);

    // setting up statemachine for reading urvalues from channels that changed its range
    m_readUrvalueState.addTransition(this, &cModuleActivist::activationContinue, &m_readUrvalueDoneState);
    m_readUrvalueDoneState.addTransition(this, &cModuleActivist::activationContinue, &m_foutParamsToDsp);
    m_readUrvalueDoneState.addTransition(this, &cModuleActivist::activationLoop, &m_readUrvalueState);

    m_readUpperRangeValueMachine.addState(&m_readUrvalueState);
    m_readUpperRangeValueMachine.addState(&m_readUrvalueDoneState);
    m_readUpperRangeValueMachine.addState(&m_foutParamsToDsp);

    m_readUpperRangeValueMachine.setInitialState(&m_readUrvalueState);

    connect(&m_readUrvalueState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::readUrvalue);
    connect(&m_readUrvalueDoneState, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::readUrvalueDone);
    connect(&m_foutParamsToDsp, &QAbstractState::entered, this, &cPower1ModuleMeasProgram::foutParamsToDsp);

}

void cPower1ModuleMeasProgram::start()
{
    if (getConfData()->m_bmovingWindow) {
        m_movingwindowFilter.setIntegrationtime(getConfData()->m_fMeasIntervalTime.m_fValue);
        connect(this, &cPower1ModuleMeasProgram::actualValues, &m_movingwindowFilter, &cMovingwindowFilter::receiveActualValues);
        connect(&m_movingwindowFilter, &cMovingwindowFilter::actualValues, this, &cPower1ModuleMeasProgram::setInterfaceActualValues);
    }
    else
        connect(this, &cPower1ModuleMeasProgram::actualValues, this, &cPower1ModuleMeasProgram::setInterfaceActualValues);
}


void cPower1ModuleMeasProgram::stop()
{
    disconnect(this, &cPower1ModuleMeasProgram::actualValues, 0, 0);
    disconnect(&m_movingwindowFilter, &cMovingwindowFilter::actualValues, this, 0);
}

void cPower1ModuleMeasProgram::generateVeinInterface()
{
    QString key;
    for (int i = 0; i < MeasPhaseCount+SumValueCount; i++) {
        QString strDescription = getPhasePowerDescription(i);
        VfModuleComponent *pActvalue = new VfModuleComponent(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                            QString("ACT_PQS%1").arg(i+1),
                                            strDescription);
        m_veinActValueList.append(pActvalue); // we add the component for our measurement
        m_pModule->veinModuleActvalueList.append(pActvalue); // and for the modules interface
    }

    m_pPQSCountInfo = new VfModuleMetaData(QString("PQSCount"), QVariant(MeasPhaseCount+SumValueCount));
    m_pModule->veinModuleMetaDataList.append(m_pPQSCountInfo);
    m_pNomFrequencyInfo =  new VfModuleMetaData(QString("NominalFrequency"), QVariant(getConfData()->m_nNominalFrequency.m_nValue));
    m_pModule->veinModuleMetaDataList.append(m_pNomFrequencyInfo);
    m_pFoutCount = new VfModuleMetaData(QString("FOUTCount"), QVariant(getConfData()->m_nFreqOutputCount));
    m_pModule->veinModuleMetaDataList.append(m_pFoutCount);
    VfModuleParameter* pFoutParameter;
    bool foutDisplayNameFound = false;
    for (int i = 0; i < getConfData()->m_nFreqOutputCount; i++) {
        // Note: Although components are 'PAR_' they are not changable currently
        pFoutParameter = new VfModuleParameter(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                                          key = QString("PAR_FOUTConstant%1").arg(i),
                                                          QString("Frequency output constant"),
                                                          QVariant(0.0));
        if(getConfData()->m_enableScpiCommands)
            pFoutParameter->setScpiInfo("CONFIGURATION",QString("M%1CONSTANT").arg(i), SCPI::isQuery, pFoutParameter->getName());

        m_FoutConstParameterList.append(pFoutParameter);
        m_pModule->m_veinModuleParameterMap[key] = pFoutParameter; // for modules use

        QString foutName = getConfData()->m_FreqOutputConfList.at(i).m_sFreqOutNameDisplayed;
        foutDisplayNameFound = !foutName.isEmpty();
        pFoutParameter = new VfModuleParameter(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                                          key = QString("PAR_FOUT%1").arg(i),
                                                          QString("Frequency output name"),
                                                          QVariant(foutName));
        if(getConfData()->m_enableScpiCommands)
            pFoutParameter->setScpiInfo("CONFIGURATION",QString("M%1OUT").arg(i), SCPI::isQuery, pFoutParameter->getName());

        m_pModule->m_veinModuleParameterMap[key] = pFoutParameter; // for modules use

        // This code seems to identify fout channels using the list positions.
        // If no scaling Information is provided we will add null pointers to keep the positions correct
        VeinStorage::AbstractComponentPtr scaleInputU;
        VeinStorage::AbstractComponentPtr scaleInputI;
        if(getConfData()->m_FreqOutputConfList.length() > i) {
            int entityIdScaleU = getConfData()->m_FreqOutputConfList.at(i).m_uscale.m_entityId;
            const VeinStorage::AbstractDatabase *storageDb = m_pModule->getStorageDb();
            QString componentNameScaleU = getConfData()->m_FreqOutputConfList.at(i).m_uscale.m_componentName;
            scaleInputU = storageDb->findComponent(entityIdScaleU, componentNameScaleU);

            int entityIdScaleI = getConfData()->m_FreqOutputConfList.at(i).m_iscale.m_entityId;
            QString componentNameScaleI = getConfData()->m_FreqOutputConfList.at(i).m_iscale.m_componentName;
            scaleInputI = storageDb->findComponent(entityIdScaleI, componentNameScaleI);
        }
        QPair<VeinStorage::AbstractComponentPtr, VeinStorage::AbstractComponentPtr> tmpScalePair(scaleInputU, scaleInputI);
        m_scalingInputs.append(tmpScalePair);
    }

    if (foutDisplayNameFound)
        generateVeinInterfaceNominalFreq();
    generateVeinInterfaceForQrefFreq();

    for(const auto &ele : qAsConst(m_scalingInputs)) {
        if(ele.first != nullptr && ele.second != nullptr) {
            connect(ele.first.get(), &VeinStorage::AbstractComponent::sigValueChange,
                    this, &cPower1ModuleMeasProgram::updatePreScaling);
            connect(ele.second.get(), &VeinStorage::AbstractComponent::sigValueChange,
                    this, &cPower1ModuleMeasProgram::updatePreScaling);
        }
    }

    // our parameters we deal with
    m_pMeasuringmodeParameter = new VfModuleParameter(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                                         key = QString("PAR_MeasuringMode"),
                                                         QString("Measuring mode"),
                                                         QVariant(getConfData()->m_sMeasuringMode.m_sValue));
    if(getConfData()->m_enableScpiCommands)
        m_pMeasuringmodeParameter->setScpiInfo("CONFIGURATION","MMODE", SCPI::isQuery|SCPI::isCmdwP, "PAR_MeasuringMode");

    cStringValidator *sValidator = new cStringValidator(getConfData()->m_sMeasmodeList);
    m_pMeasuringmodeParameter->setValidator(sValidator);
    m_pModule->m_veinModuleParameterMap[key] = m_pMeasuringmodeParameter; // for modules use

    if(getConfData()->m_enableScpiCommands)
        m_pModule->scpiCommandList.append(new ScpiVeinComponentInfo("CONFIGURATION", QString("MMODE:CATALOG"),
                                                                    SCPI::isQuery,
                                                                    m_pMeasuringmodeParameter->getName(),
                                                                    SCPI::isCatalog));

    m_pMModePhaseSelectParameter = new VfModuleParameter(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                                         key = QString("PAR_MeasModePhaseSelect"),
                                                         QString("Active phases selection mask"),
                                                         getInitialPhaseOnOffVeinVal());
    if(getConfData()->m_enableScpiCommands)
        m_pMModePhaseSelectParameter->setScpiInfo("CONFIGURATION","MEASMODEPHASESELECT", SCPI::isQuery|SCPI::isCmdwP, "PAR_MeasModePhaseSelect");
    m_MModePhaseSelectValidator = new cStringValidator("");
    m_pMModePhaseSelectParameter->setValidator(m_MModePhaseSelectValidator);
    m_pModule->m_veinModuleParameterMap[key] = m_pMModePhaseSelectParameter; // for modules use

    m_MModeCanChangePhaseMask = new VfModuleComponent(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                     QString("ACT_CanChangePhaseMask"),
                                     QString("Boolean indicator that current measurement mode can change phase mask"),
                                     QVariant(false) );
    m_veinActValueList.append(m_MModeCanChangePhaseMask); // we add the component for our measurement
    m_pModule->veinModuleActvalueList.append(m_MModeCanChangePhaseMask); // and for the modules interface

    m_MModePowerDisplayName = new VfModuleComponent(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                                   QString("ACT_PowerDisplayName"),
                                                   QString("Power display name (P/Q/S)"),
                                                   QVariant("") );
    m_veinActValueList.append(m_MModePowerDisplayName);
    m_pModule->veinModuleActvalueList.append(m_MModePowerDisplayName);

    m_MModeMaxMeasSysCount = new VfModuleComponent(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                                     QString("ACT_MaxMeasSysCount"),
                                                     QString("Number of max measurement systems for current measurement mode"),
                                                     QVariant(3) );
    m_veinActValueList.append(m_MModeMaxMeasSysCount); // we add the component for our measurement
    m_pModule->veinModuleActvalueList.append(m_MModeMaxMeasSysCount); // and for the modules interface

    m_MModesTypes = new VfModuleComponent(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                          QString("INF_ModeTypes"),
                                          QString("Display names (P/Q/S) of available modes"),
                                          QVariant(QStringList()));
    m_pModule->veinModuleComponentList.append(m_MModesTypes);

    QVariant val;
    QString s, unit;
    bool btime = (getConfData()->m_sIntegrationMode == "time");
    if (btime) {
        val = QVariant(getConfData()->m_fMeasIntervalTime.m_fValue);
        s = QString("Integration time");
        unit = QString("s");
    }
    else {
        val = QVariant(getConfData()->m_nMeasIntervalPeriod.m_nValue);
        s = QString("Integration period");
        unit = QString("period");
    }

    m_pIntegrationParameter = new VfModuleParameter(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                                       key = QString("PAR_Interval"),
                                                       s,
                                                       val);
    m_pIntegrationParameter->setUnit(unit);
    if (btime) {
        if(getConfData()->m_enableScpiCommands)
            m_pIntegrationParameter->setScpiInfo("CONFIGURATION","TINTEGRATION", SCPI::isQuery|SCPI::isCmdwP, "PAR_Interval");
        m_pIntegrationParameter->setValidator(new cDoubleValidator(1.0, 100.0, 0.5));
    }
    else {
        if(getConfData()->m_enableScpiCommands)
            m_pIntegrationParameter->setScpiInfo("CONFIGURATION","TPERIOD", SCPI::isQuery|SCPI::isCmdwP, "PAR_Interval");
        m_pIntegrationParameter->setValidator(new cIntValidator(5, 5000, 1));
    }
    m_pModule->m_veinModuleParameterMap[key] = m_pIntegrationParameter; // for modules use

    m_pMeasureSignal = new VfModuleComponent(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                                QString("SIG_Measuring"),
                                                QString("Signal indicating measurement activity"),
                                                QVariant(0));

    m_pModule->veinModuleComponentList.append(m_pMeasureSignal);
}


void cPower1ModuleMeasProgram::setDspVarList()
{
    int samples = m_pModule->getSharedChannelRangeObserver()->getSamplesPerPeriod();
    m_dspVars.setupVarList(m_dspInterface.get(), getConfData(), samples);
    m_ModuleActualValues.resize(m_dspVars.getActualValues()->getSize()); // we provide a vector for generated actual values
    m_nDspMemUsed = m_dspVars.getMemUsed();
}


MeasSystemChannels cPower1ModuleMeasProgram::getMeasChannelUIPairs()
{
    ChannelRangeObserver::SystemObserverPtr observer = m_pModule->getSharedChannelRangeObserver();
    MeasSystemChannels measChannelUIPairList;
    for(const auto &measChannelPair : qAsConst(getConfData()->m_sMeasSystemList)) {
        QStringList channelPairSplit = measChannelPair.split(',');
        MeasSystemChannel measChannel;
        measChannel.voltageChannel = observer->getChannel(channelPairSplit.at(0))->getDspChannel();
        measChannel.currentChannel = observer->getChannel(channelPairSplit.at(1))->getDspChannel();
        measChannelUIPairList.append(measChannel);
    }
    return measChannelUIPairList;
}

QStringList cPower1ModuleMeasProgram::setupMeasModes(DspChainIdGen& dspChainGen)
{
    QStringList dspMModesCommandList = Power1DspCmdGenerator::getCmdsInitOutputVars();
    cPower1ModuleConfigData *confdata = getConfData();
    int measSytemCount = confdata->m_sMeasSystemList.count();
    MeasSystemChannels measChannelUIPairList = getMeasChannelUIPairs();
    MeasModeBroker measBroker(Power1DspModeFunctionCatalog::get(measSytemCount), dspChainGen);
    for (int i = 0; i < confdata->m_nMeasModeCount; i++) {
        cMeasModeInfo mInfo = MeasModeCatalog::getInfo(confdata->m_sMeasmodeList.at(i));
        MeasModeBroker::BrokerReturn brokerReturn = measBroker.getMeasMode(mInfo.getName(), measChannelUIPairList);
        dspMModesCommandList.append(brokerReturn.dspCmdList);
        std::shared_ptr<MeasMode> mode = std::make_shared<MeasMode>(mInfo.getName(),
                                                                    brokerReturn.dspSelectCode,
                                                                    measSytemCount,
                                                                    std::move(brokerReturn.phaseStrategy));
        MeasModePhasePersistency::setMeasModePhaseFromConfig(mode, confdata->m_measmodePhaseList);
        m_measModeSelector.addMode(mode);
    }
    dspMModesCommandList.append(Power1DspCmdGenerator::getCmdsSumAndAverage(dspChainGen));
    m_measModeSelector.tryChangeMode(confdata->m_sMeasuringMode.m_sValue);

    return dspMModesCommandList;
}

void cPower1ModuleMeasProgram::setDspCmdList()
{
    DspChainIdGen dspChainGen;
    const cPower1ModuleConfigData *confdata = getConfData();

    QStringList dspMModesCommandList = setupMeasModes(dspChainGen);
    std::shared_ptr<MeasMode> mode = m_measModeSelector.getCurrMode();
    int samples = m_pModule->getSharedChannelRangeObserver()->getSamplesPerPeriod();
    QStringList dspInitVarsList = Power1DspCmdGenerator::getCmdsInitVars(mode,
                                                                         samples,
                                                                         calcTiTime(),
                                                                         confdata->m_sIntegrationMode == "time",
                                                                         dspChainGen);
    QStringList dspFreqCmds = Power1DspCmdGenerator::getCmdsFreqOutput(confdata, m_FoutInfoMap, irqNr, dspChainGen);

    // sequence here is important
    m_dspInterface->addCycListItems(dspInitVarsList);
    m_dspInterface->addCycListItems(dspMModesCommandList);
    m_dspInterface->addCycListItems(dspFreqCmds);
}

void cPower1ModuleMeasProgram::deleteDspCmdList()
{
    m_dspInterface->clearCmdList();
}

void cPower1ModuleMeasProgram::catchInterfaceAnswer(quint32 msgnr, quint8 reply, QVariant answer)
{
    if (msgnr == 0) { // 0 was reserved for async. messages
        QString sintnr = answer.toString().section(':', 1, 1);
        QString device = answer.toString().section(':', 0, 0);
        if(device == "DSPINT") {
            // we got an interrupt from our cmd chain and have to fetch our actual values
            // but we synchronize on ranging process
            if (m_bActive && !m_dataAcquisitionMachine.isRunning()) // in case of deactivation in progress, no dataaquisition
                m_dataAcquisitionMachine.start();
        }
        else {
            int service = sintnr.toInt();
            switch (service)
            {
            case irqNr+1:
            case irqNr+2:
            case irqNr+3:
            case irqNr+4:
            case irqNr+5:
            case irqNr+6:
            // we got a sense:channel:range notifier
            // let's look what to do
                QString s = m_NotifierInfoHash[service];
                readUrvalueList.append(s);
                if (!m_readUpperRangeValueMachine.isRunning())
                    m_readUpperRangeValueMachine.start();
                break;
            }
        }
    }
    else {
        // maybe other objexts share the same dsp interface
        if (m_MsgNrCmdList.contains(msgnr)) {
            int cmd = m_MsgNrCmdList.take(msgnr);
            switch (cmd)
            {
            case sendrmident:
                if (reply == ack)
                    emit activationContinue();
                else
                    notifyError(rmidentErrMSG);
                break;

            case setchannelrangenotifier:
                if (reply == ack) { // we only continue pcb server acknowledges
                    m_NotifierInfoHash[m_notifierNr] = infoRead;
                    emit activationContinue();
                }
                else
                    notifyError(registerpcbnotifierErrMsg);
                break;

            case readurvalue:
            {
                if (reply == ack) {
                    double urvalue = answer.toDouble();
                    cMeasChannelInfo mi = m_measChannelInfoHash.take(readUrvalueInfo);
                    mi.m_fUrValue = urvalue;
                    m_measChannelInfoHash[readUrvalueInfo] = mi;
                    emit activationContinue();
                }
                else
                    notifyError(readrangeurvalueErrMsg);
                break;
            }

            case setqrefnominalpower:
                if (reply != ack) // we ignore ack
                    notifyError(writedspmemoryErrMsg);
                break;

            case claimpgrmem:
                if (reply == ack)
                    emit activationContinue();
                else
                    notifyError(claimresourceErrMsg);
                break;

            case claimusermem:
                if (reply == ack)
                    emit activationContinue();
                else
                    notifyError(claimresourceErrMsg);
                break;

            case varlist2dsp:
                if (reply == ack)
                    emit activationContinue();
                else
                    notifyError(dspvarlistwriteErrMsg);
                break;

            case cmdlist2dsp:
                if (reply == ack)
                    emit activationContinue();
                else
                    notifyError(dspcmdlistwriteErrMsg);
                break;

            case activatedsp:
                if (reply == ack)
                    emit activationContinue();
                else
                    notifyError(dspactiveErrMsg);
                break;

            case claimresourcesource:
                if (reply == ack)
                    emit activationContinue();
                else
                    notifyError(claimresourceErrMsg);
                break;

            case readsourcechannelalias:
                if (reply == ack) {
                    QString alias = answer.toString();
                    cFoutInfo fi = m_FoutInfoMap.take(infoRead);
                    fi.alias = alias;
                    m_FoutInfoMap[infoRead] = fi;
                    emit activationContinue();
                }
                else
                    notifyError(readaliasErrMsg);
                break;

            case readsourcechanneldspchannel:
                if (reply == ack) {
                    int chnnr = answer.toInt();
                    cFoutInfo fi = m_FoutInfoMap.take(infoRead);
                    fi.dspFoutChannel = chnnr;
                    m_FoutInfoMap[infoRead] = fi;
                    emit activationContinue();
                }
                else
                    notifyError(readdspchannelErrMsg);
                break;

            case readsourceformfactor:
                if (reply == ack) {
                    double ffac = answer.toDouble();
                    cFoutInfo fi = m_FoutInfoMap.take(infoRead);
                    fi.formFactor = ffac;
                    m_FoutInfoMap[infoRead] = fi;
                    emit activationContinue();
                }
                else
                    notifyError(readFormFactorErrMsg);
                break;

            case writeparameter:
                if (reply == ack) // we ignore ack
                    ;
                else
                    notifyError(writedspmemoryErrMsg);
                break;

            case deactivatedsp:
                if (reply == ack)
                    emit deactivationContinue();
                else
                    notifyError(dspdeactiveErrMsg);
                break;
            case freepgrmem:
                if (reply == ack)
                    emit deactivationContinue();
                else
                    notifyError(freeresourceErrMsg);
                break;

            case freeusermem:
            case freeresourcesource:
                if (reply == ack)
                    emit deactivationContinue();
                else
                    notifyError(freeresourceErrMsg);
                break;

            case unregisterrangenotifiers:
                if (reply == ack) // we only continue pcb server acknowledges
                    emit deactivationContinue();
                else
                    notifyError(unregisterpcbnotifierErrMsg);
                break;

            case dataaquistion:
                if (reply == ack)
                    emit dataAquisitionContinue();
                else {
                    m_dataAcquisitionMachine.stop();
                    notifyError(dataaquisitionErrMsg);
                }
                break;
            }
        }
    }
}

cPower1ModuleConfigData *cPower1ModuleMeasProgram::getConfData()
{
    return qobject_cast<cPower1ModuleConfiguration*>(m_pConfiguration.get())->getConfigurationData();
}

void cPower1ModuleMeasProgram::setActualValuesNames()
{
    QString powIndicator = "123S"; // (MeasPhaseCount ???)
    const cMeasModeInfo mminfo = MeasModeCatalog::getInfo(getConfData()->m_sMeasuringMode.m_sValue);
    for (int i = 0; i < MeasPhaseCount+SumValueCount; i++) {
        m_veinActValueList.at(i)->setChannelName(QString("%1%2").arg(mminfo.getActvalName()).arg(powIndicator[i]));
        m_veinActValueList.at(i)->setUnit(mminfo.getUnitName());
    }
    m_pModule->exportMetaData();
}

void cPower1ModuleMeasProgram::setSCPIMeasInfo()
{
    if(getConfData()->m_enableScpiCommands) {
        for (int i = 0; i < MeasPhaseCount+SumValueCount; i++)
            m_veinActValueList.at(i)->setScpiInfo("MEASURE", m_veinActValueList.at(i)->getChannelName(), SCPI::isCmdwP, m_veinActValueList.at(i)->getName());
    }
}

void cPower1ModuleMeasProgram::setFoutMetaInfo()
{
    for (int i = 0; i < getConfData()->m_FreqOutputConfList.count(); i++)
        m_FoutConstParameterList.at(i)->setChannelName(m_FoutInfoMap[getConfData()->m_FreqOutputConfList.at(i).m_sName].alias);
}

void cPower1ModuleMeasProgram::setInterfaceActualValues(QVector<float> *actualValues)
{
    if (m_bActive) { // maybe we are deactivating !!!!
        for (int i = 0; i < MeasPhaseCount+SumValueCount; i++) {
            if(m_measModeSelector.getCurrMode()->getName() != "QREF")
                m_veinActValueList.at(i)->setValue(QVariant((double)actualValues->at(i)));
            else
                m_veinActValueList.at(i)->setValue(QVariant((double)0.0));
        }
    }
}

void cPower1ModuleMeasProgram::resourceManagerConnect()
{
    // as this is our entry point when activating the module, we do some initialization first
    m_measChannelInfoHash.clear(); // we build up a new channel info hash
    cMeasChannelInfo mi;
    for(const auto &measSystem : qAsConst(getConfData()->m_sMeasSystemList)) {
        QStringList sl = measSystem.split(',');
        for (int j = 0; j < sl.count(); j++) {
            QString s = sl.at(j);
            if (!m_measChannelInfoHash.contains(s)) // did we find a new measuring channel ?
                m_measChannelInfoHash[s] = mi; // then lets add it
        }
    }

    m_FoutInfoMap.clear();
    cFoutInfo fi;
    for (int i = 0; i < getConfData()->m_nFreqOutputCount; i++) {
        QString name = getConfData()->m_FreqOutputConfList.at(i).m_sName;
        if (!m_FoutInfoMap.contains(name))
            m_FoutInfoMap[name] = fi;
    }

    m_NotifierInfoHash.clear();

    // we have to instantiate a working resource manager interface
    // so first we try to get a connection to resource manager over proxy
    m_rmClient = Zera::Proxy::getInstance()->getConnectionSmart(m_pModule->getNetworkConfig()->m_rmServiceConnectionInfo,
                                                                m_pModule->getNetworkConfig()->m_tcpNetworkFactory);
    // and then we set resource manager interface's connection
    m_rmInterface.setClientSmart(m_rmClient);
    m_resourceManagerConnectState.addTransition(m_rmClient.get(), &Zera::ProxyClient::connected, &m_IdentifyState);
    connect(&m_rmInterface, &AbstractServerInterface::serverAnswer, this, &cPower1ModuleMeasProgram::catchInterfaceAnswer);
    Zera::Proxy::getInstance()->startConnectionSmart(m_rmClient);
}

void cPower1ModuleMeasProgram::sendRMIdent()
{
    m_MsgNrCmdList[m_rmInterface.rmIdent(QString("Power1Module%1").arg(m_pModule->getModuleNr()))] = sendrmident;
}

void cPower1ModuleMeasProgram::claimResourcesSource()
{
    infoReadList = m_FoutInfoMap.keys(); // we have to read information for all freq outputs in this list
    if(!infoReadList.isEmpty())
        emit activationContinue();
    else
        emit activationSkip();
}

void cPower1ModuleMeasProgram::claimResourceSource()
{
    infoRead = infoReadList.takeLast();
    m_MsgNrCmdList[m_rmInterface.setResource("SOURCE", infoRead, 1)] = claimresourcesource;
}

void cPower1ModuleMeasProgram::claimResourceSourceDone()
{
    if (infoReadList.isEmpty())
        emit activationContinue();
    else
        emit activationLoop();
}

void cPower1ModuleMeasProgram::pcbserverConnect4measChannels()
{
    // we have to connect to all ports....
    infoReadList = m_measChannelInfoHash.keys(); // so first we look for our different pcb sockets for sense
    m_nConnectionCount = infoReadList.count();

    for (int i = 0; i < infoReadList.count(); i++) {
        QString key = infoReadList.at(i);
        cMeasChannelInfo mi = m_measChannelInfoHash.take(key);
        Zera::ProxyClientPtr pcbClient = Zera::Proxy::getInstance()->getConnectionSmart(
            m_pModule->getNetworkConfig()->m_pcbServiceConnectionInfo,
            m_pModule->getNetworkConfig()->m_tcpNetworkFactory);
        Zera::PcbInterfacePtr pcbInterface = std::make_shared<Zera::cPCBInterface>();
        pcbInterface->setClientSmart(pcbClient);
        mi.pcbIFace = pcbInterface;
        m_measChannelInfoHash[key] = mi;
        connect(pcbClient.get(), &Zera::ProxyClient::connected, this, &cPower1ModuleMeasProgram::monitorConnection); // here we wait until all connections are established
        connect(pcbInterface.get(), &AbstractServerInterface::serverAnswer, this, &cPower1ModuleMeasProgram::catchInterfaceAnswer);
        Zera::Proxy::getInstance()->startConnectionSmart(pcbClient);
    }
}

void cPower1ModuleMeasProgram::pcbserverConnect4freqChannels()
{
    infoReadList = m_FoutInfoMap.keys(); // and then  we look for our different pcb sockets for source
    if (infoReadList.count() > 0) {
        m_nConnectionCount += infoReadList.count();
        for (int i = 0; i < infoReadList.count(); i++) {
            QString key = infoReadList.at(i);
            cFoutInfo fi = m_FoutInfoMap.take(key);
            Zera::ProxyClientPtr pcbClient = Zera::Proxy::getInstance()->getConnectionSmart(
                m_pModule->getNetworkConfig()->m_pcbServiceConnectionInfo,
                m_pModule->getNetworkConfig()->m_tcpNetworkFactory);
            Zera::PcbInterfacePtr pcbInterface = std::make_shared<Zera::cPCBInterface>();
            pcbInterface->setClientSmart(pcbClient);
            fi.pcbIFace = pcbInterface;
            fi.name = key;
            m_FoutInfoMap[key] = fi;
            connect(pcbClient.get(), &Zera::ProxyClient::connected, this, &cPower1ModuleMeasProgram::monitorConnection); // here we wait until all connections are established
            connect(pcbInterface.get(), &AbstractServerInterface::serverAnswer, this, &cPower1ModuleMeasProgram::catchInterfaceAnswer);
            Zera::Proxy::getInstance()->startConnectionSmart(pcbClient);
        }
    }
    else
        emit activationContinue();
}

void cPower1ModuleMeasProgram::readSourceChannelInformation()
{
    if (getConfData()->m_nFreqOutputCount > 0) // we only have to read information if really configured
    {
        infoReadList = m_FoutInfoMap.keys(); // we have to read information for all channels in this list
        emit activationContinue();
    }
    else
        emit activationSkip();
}

void cPower1ModuleMeasProgram::readSourceChannelAlias()
{
    infoRead = infoReadList.takeFirst();
    m_MsgNrCmdList[m_FoutInfoMap[infoRead].pcbIFace->getAliasSource(infoRead)] = readsourcechannelalias;
}


void cPower1ModuleMeasProgram::readSourceDspChannel()
{
    m_MsgNrCmdList[m_FoutInfoMap[infoRead].pcbIFace->getDSPChannelSource(infoRead)] = readsourcechanneldspchannel;
}


void cPower1ModuleMeasProgram::readSourceFormFactor()
{
    m_MsgNrCmdList[m_FoutInfoMap[infoRead].pcbIFace->getFormFactorSource(infoRead)] = readsourceformfactor;
}


void cPower1ModuleMeasProgram::readSourceChannelInformationDone()
{
    if (infoReadList.isEmpty())
        emit activationContinue();
    else
        emit activationLoop();
}


void cPower1ModuleMeasProgram::setSenseChannelRangeNotifiers()
{
    m_notifierNr = irqNr;
    infoReadList = m_measChannelInfoHash.keys(); // we have to set notifier for each channel we are working on
    emit activationContinue();
}


void cPower1ModuleMeasProgram::setSenseChannelRangeNotifier()
{
    infoRead = infoReadList.takeFirst();
    m_notifierNr++;
    // we will get irq+1 .. irq+6 for notification if ranges change
    m_MsgNrCmdList[ m_measChannelInfoHash[infoRead].pcbIFace->registerNotifier(QString("sens:%1:rang?").arg(infoRead), m_notifierNr)] = setchannelrangenotifier;

}


void cPower1ModuleMeasProgram::setSenseChannelRangeNotifierDone()
{
    if (infoReadList.isEmpty())
        emit activationContinue();
    else
        emit activationLoop();
}


void cPower1ModuleMeasProgram::dspserverConnect()
{
    m_dspClient = Zera::Proxy::getInstance()->getConnectionSmart(m_pModule->getNetworkConfig()->m_dspServiceConnectionInfo,
                                                                 m_pModule->getNetworkConfig()->m_tcpNetworkFactory);
    m_dspInterface->setClientSmart(m_dspClient);
    m_dspserverConnectState.addTransition(m_dspClient.get(), &Zera::ProxyClient::connected, &m_claimPGRMemState);
    connect(m_dspInterface.get(), &AbstractServerInterface::serverAnswer, this, &cPower1ModuleMeasProgram::catchInterfaceAnswer);
    Zera::Proxy::getInstance()->startConnectionSmart(m_dspClient);
}


void cPower1ModuleMeasProgram::claimPGRMem()
{
    setDspVarList(); // !!! sample rate must be fetched before (currently in state m_readSampleRateState)
    setDspCmdList();
    m_MsgNrCmdList[m_rmInterface.setResource("DSP1", "PGRMEMC", m_dspInterface->cmdListCount())] = claimpgrmem;
}


void cPower1ModuleMeasProgram::claimUSERMem()
{
   m_MsgNrCmdList[m_rmInterface.setResource("DSP1", "USERMEM", m_nDspMemUsed)] = claimusermem;
}


void cPower1ModuleMeasProgram::varList2DSP()
{
    m_MsgNrCmdList[m_dspInterface->varList2Dsp()] = varlist2dsp;
}


void cPower1ModuleMeasProgram::cmdList2DSP()
{
    m_MsgNrCmdList[m_dspInterface->cmdList2Dsp()] = cmdlist2dsp;
}


void cPower1ModuleMeasProgram::activateDSP()
{
    m_MsgNrCmdList[m_dspInterface->activateInterface()] = activatedsp; // aktiviert die var- und cmd-listen im dsp
}

void cPower1ModuleMeasProgram::setModeTypesComponent()
{
    const QHash<QString, std::shared_ptr<MeasMode>> &measModes = m_measModeSelector.getModes();
    QStringList measModeTypes;
    for(auto iter=measModes.constBegin(); iter!=measModes.constEnd(); iter++) {
        std::shared_ptr<MeasMode> measMode = iter.value();
        const cMeasModeInfo modeInfo = MeasModeCatalog::getInfo(measMode->getName());
        QString modeTypeName = modeInfo.getActvalName();
        if(!measModeTypes.contains(modeTypeName))
            measModeTypes.append(modeTypeName);
    }
    measModeTypes.sort();
    m_MModesTypes->setValue(measModeTypes);
}

void cPower1ModuleMeasProgram::activateDSPdone()
{
    m_bActive = true;
    std::shared_ptr<MeasMode> mode = m_measModeSelector.getCurrMode();
    updatePhaseMaskVeinComponents(mode);
    setActualValuesNames();
    setSCPIMeasInfo();
    setFoutMetaInfo();
    setModeTypesComponent();
    m_pMeasureSignal->setValue(QVariant(1));

    if (getConfData()->m_sIntegrationMode == "time")
        connect(m_pIntegrationParameter, &VfModuleComponent::sigValueChanged, this, &cPower1ModuleMeasProgram::newIntegrationtime);
    else
        connect(m_pIntegrationParameter, &VfModuleComponent::sigValueChanged, this, &cPower1ModuleMeasProgram::newIntegrationPeriod);
    connect(m_pMeasuringmodeParameter, &VfModuleComponent::sigValueChanged, this, &cPower1ModuleMeasProgram::newMeasMode);
    connect(m_pMModePhaseSelectParameter, &VfModuleComponent::sigValueChanged, this, &cPower1ModuleMeasProgram::newPhaseList);
    connect(&m_measModeSelector, &MeasModeSelector::sigTransactionOk,
            this, &cPower1ModuleMeasProgram::onModeTransactionOk);
    if (getConfData()->supportsVariableQrefFrequency())
        connect(m_QREFFrequencyParameter, &VfModuleComponent::sigValueChanged,
                this, &cPower1ModuleMeasProgram::newQRefFrequency);
    if (m_pNominalFrequency)
        connect(m_pNominalFrequency,&VfModuleParameter::sigValueChanged, this, &cPower1ModuleMeasProgram::newNominalFrequency);

    readUrvalueList = m_measChannelInfoHash.keys(); // once we read all actual range urvalues
    if (!m_readUpperRangeValueMachine.isRunning())
        m_readUpperRangeValueMachine.start();

    emit activated();
}

void cPower1ModuleMeasProgram::freePGRMem()
{
    m_dataAcquisitionMachine.stop();
    m_bActive = false;
    Zera::Proxy::getInstance()->releaseConnectionSmart(m_dspClient);
    deleteDspCmdList();

    m_MsgNrCmdList[m_rmInterface.freeResource("DSP1", "PGRMEMC")] = freepgrmem;
}


void cPower1ModuleMeasProgram::freeUSERMem()
{
    m_MsgNrCmdList[m_rmInterface.freeResource("DSP1", "USERMEM")] = freeusermem;
}


void cPower1ModuleMeasProgram::freeFreqOutputs()
{
    if (getConfData()->m_nFreqOutputCount > 0) // we only have to read information if really configured
    {
        infoReadList = m_FoutInfoMap.keys(); // we have to read information for all channels in this list
        emit deactivationContinue();
    }
    else
        emit deactivationSkip();
}


void cPower1ModuleMeasProgram::freeFreqOutput()
{
    infoRead = infoReadList.takeFirst();
    m_MsgNrCmdList[m_rmInterface.freeResource("SOURCE", infoRead)] = freeresourcesource;
}


void cPower1ModuleMeasProgram::freeFreqOutputDone()
{
    if (infoReadList.isEmpty())
        emit deactivationContinue();
    else
        emit deactivationLoop();
}


void cPower1ModuleMeasProgram::resetNotifiers()
{
    infoReadList = m_measChannelInfoHash.keys();
    emit deactivationContinue();
}


void cPower1ModuleMeasProgram::resetNotifier()
{
    infoRead = infoReadList.takeFirst();
    m_MsgNrCmdList[m_measChannelInfoHash[infoRead].pcbIFace->unregisterNotifiers()] = unregisterrangenotifiers;
}


void cPower1ModuleMeasProgram::resetNotifierDone()
{
    if (infoReadList.isEmpty())
        emit deactivationContinue();
    else
        emit deactivationLoop();
}


void cPower1ModuleMeasProgram::deactivateDSPdone()
{
    disconnect(&m_rmInterface, 0, this, 0);
    disconnect(m_dspInterface.get(), 0, this, 0);

    emit deactivated();
}


void cPower1ModuleMeasProgram::dataAcquisitionDSP()
{
    m_pMeasureSignal->setValue(QVariant(0));
    if(m_bActive)
        m_MsgNrCmdList[m_dspInterface->dataAcquisition(m_dspVars.getActualValues())] = dataaquistion; // we start our data aquisition now
}


void cPower1ModuleMeasProgram::dataReadDSP()
{
    if (m_bActive) {
        m_ModuleActualValues = m_dspVars.getActualValues()->getData();
        emit actualValues(&m_ModuleActualValues); // and send them
        m_pMeasureSignal->setValue(QVariant(1)); // signal measuring
    }
}


void cPower1ModuleMeasProgram::readUrvalue()
{
    readUrvalueInfo = readUrvalueList.takeFirst(); // we have the channel information now
    m_MsgNrCmdList[m_measChannelInfoHash[readUrvalueInfo].pcbIFace->getUrvalue(readUrvalueInfo)] = readurvalue;
}


void cPower1ModuleMeasProgram::readUrvalueDone()
{
    if (readUrvalueList.isEmpty())
        emit activationContinue();
    else
        emit activationLoop();
}

cPower1ModuleMeasProgram::RangeMaxVals cPower1ModuleMeasProgram::calcMaxRangeValues(std::shared_ptr<MeasMode> mode)
{
    RangeMaxVals maxVals;
    for (int i = 0; i < getConfData()->m_sMeasSystemList.count(); i++) {
        if(mode->isPhaseActive(i)) {
            QStringList sl = getConfData()->m_sMeasSystemList.at(i).split(',');
            double rangeFullVal = m_measChannelInfoHash[sl.at(0)].m_fUrValue;
            if (rangeFullVal > maxVals.maxU)
                maxVals.maxU = rangeFullVal;
            rangeFullVal = m_measChannelInfoHash[sl.at(1)].m_fUrValue;
            if (rangeFullVal > maxVals.maxI)
                maxVals.maxI = rangeFullVal;
        }
    }
    return maxVals;
}

void cPower1ModuleMeasProgram::setNominalPowerForQref()
{
    std::shared_ptr<MeasMode> mode = m_measModeSelector.getCurrMode();
    RangeMaxVals maxVals = calcMaxRangeValues(mode);
    double pmax = maxVals.maxU * maxVals.maxI; // MQREF
    if (getConfData()->supportsVariableQrefFrequency()) {
        constexpr double nominalFrequencyKhz = 200;
        pmax = pmax * getConfData()->m_qrefFrequency.m_fValue / nominalFrequencyKhz;
    }
    QString datalist = QString("NOMPOWER:%1;").arg(pmax, 0, 'g', 9);
    m_dspVars.getNominalPower()->setVarData(datalist);
    m_MsgNrCmdList[m_dspInterface->dspMemoryWrite(m_dspVars.getNominalPower())] = setqrefnominalpower;
}

void cPower1ModuleMeasProgram::generateVeinInterfaceForQrefFreq()
{
    if(getConfData()->supportsVariableQrefFrequency()) {
        const QString paramLabel = "PAR_FOUT_QREF_FREQ";
        m_QREFFrequencyParameter = new VfModuleParameter(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                                        paramLabel,
                                                        QString("Fixed frequency output mode (QREF): frequency value"),
                                                        QVariant(getConfData()->m_qrefFrequency.m_fValue));
        const QString unit = "kHz";
        m_QREFFrequencyParameter->setUnit(unit);
        if(getConfData()->m_enableScpiCommands)
            m_QREFFrequencyParameter->setScpiInfo("CONFIGURATION",QString("FIXFREQ"), SCPI::isQuery|SCPI::isCmdwP, m_QREFFrequencyParameter->getName());
        cDoubleValidator *validator = new cDoubleValidator(0.001, 200.0, 0.001);
        m_QREFFrequencyParameter->setValidator(validator);
        m_pModule->m_veinModuleParameterMap[paramLabel] = m_QREFFrequencyParameter; // for modules use
    }
}

void POWER1MODULE::cPower1ModuleMeasProgram::generateVeinInterfaceNominalFreq()
{
    QString key = "PAR_FOUT_NOMINAL_FREQ";
    m_pNominalFrequency = new VfModuleParameter(m_pModule->getEntityId(), m_pModule->getValidatorEventSystem(),
                                                key,
                                                QString("Nominal frequency"),
                                                QVariant(getConfData()->m_nNominalFrequency.m_nValue));
    m_pNominalFrequency->setUnit("Hz");
    m_pNominalFrequency->setValidator(new cIntValidator(10000, 200000, 1));
    m_pModule->m_veinModuleParameterMap[key] = m_pNominalFrequency; // for modules use
    connect(m_pNominalFrequency, &VfModuleParameter::sigValueChanged, this, &cPower1ModuleMeasProgram::foutParamsToDsp);
    if(getConfData()->m_enableScpiCommands)
        m_pNominalFrequency->setScpiInfo("CONFIGURATION",QString("NOMINAL_FREQ"), SCPI::isQuery|SCPI::isCmdwP, "PAR_FOUT_NOMINAL_FREQ");
}

void cPower1ModuleMeasProgram::foutParamsToDsp()
{
    std::shared_ptr<MeasMode> mode = m_measModeSelector.getCurrMode();
    RangeMaxVals maxVals = calcMaxRangeValues(mode);
    double cfak = mode->getActiveMeasSysCount();
    double nomFrequency = getConfData()->m_nNominalFrequencyDefault.m_nValue;
    if(m_pNominalFrequency && mode->getName() != "QREF")
        nomFrequency = m_pNominalFrequency->getValue().toDouble();
    double constantImpulsePerWs = nomFrequency / (cfak * maxVals.maxU * maxVals.maxI);

    bool isValidConstant = !isinf(constantImpulsePerWs);
    if (isValidConstant && getConfData()->m_nFreqOutputCount > 0) { // dsp-interface will crash for missing parts...
        QString datalist = "FREQSCALE:";
        for (int i = 0; i<getConfData()->m_nFreqOutputCount; i++) {
            double frScale;
            cFoutInfo fi = m_FoutInfoMap[getConfData()->m_FreqOutputConfList.at(i).m_sName];
            frScale = fi.formFactor * constantImpulsePerWs;

            if(m_scalingInputs.length() > i) {
                if(m_scalingInputs.at(i).first && m_scalingInputs.at(i).second) {
                    double scaleU = m_scalingInputs.at(i).first->getValue().toDouble();
                    double scaleI = m_scalingInputs.at(i).second->getValue().toDouble();
                    double scaleTotal = scaleU * scaleI;
                    frScale = frScale * scaleTotal;
                }
            }
            datalist += QString("%1,").arg(frScale, 0, 'g', 9);
        }
        datalist.resize(datalist.size()-1);
        datalist += ";";
        m_dspVars.getFreqScale()->setVarData(datalist);
        m_MsgNrCmdList[m_dspInterface->dspMemoryWrite(m_dspVars.getFreqScale())] = writeparameter;
    }
    setFoutPowerModes();
    double constantImpulsePerKwh = 3600.0 * 1000.0 * constantImpulsePerWs; // imp./kwh
    for (int i = 0; i < getConfData()->m_nFreqOutputCount; i++) {
        // calculate prescaling factor for Fout
        if(m_scalingInputs.length() > i) {
            if(m_scalingInputs.at(i).first && m_scalingInputs.at(i).second){
                double scaleU = m_scalingInputs.at(i).first->getValue().toDouble();
                double scaleI = m_scalingInputs.at(i).second->getValue().toDouble();
                double scaleTotal = scaleU * scaleI;
                constantImpulsePerKwh = constantImpulsePerKwh * scaleTotal;
            }
        }
        QString key = getConfData()->m_FreqOutputConfList.at(i).m_sName;
        cFoutInfo fi = m_FoutInfoMap[key];
        if(isValidConstant) {
            m_MsgNrCmdList[fi.pcbIFace->setConstantSource(fi.name, constantImpulsePerKwh)] = writeparameter;
            m_FoutConstParameterList.at(i)->setValue(constantImpulsePerKwh);
        }
        else
            m_FoutConstParameterList.at(i)->setValue(0.0);
    }

    setNominalPowerForQref();
}

void cPower1ModuleMeasProgram::setFoutPowerModes()
{
    QList<QString> keylist = m_FoutInfoMap.keys();
    for (int i = 0; i < keylist.count(); i++) {
        QString powtype;
        int foutmode = getConfData()->m_FreqOutputConfList.at(i).m_nFoutMode;
        switch (foutmode)
        {
        case posPower:
            powtype = "+";
            break;
        case negPower:
            powtype = "-";
            break;
        default:
            powtype = "";
        }
        powtype += MeasModeCatalog::getInfo(getConfData()->m_sMeasuringMode.m_sValue).getActvalName();
        cFoutInfo fi = m_FoutInfoMap[keylist.at(i)];
        m_MsgNrCmdList[fi.pcbIFace->setPowTypeSource(fi.name, powtype)] = writeparameter;
    }
}

QString cPower1ModuleMeasProgram::getInitialPhaseOnOffVeinVal()
{
    cPower1ModuleConfigData *confData = getConfData();
    QString phaseOnOff;
    for(int phase=0; phase<confData->m_sMeasSystemList.count(); phase++)
        phaseOnOff.append("1");
    return phaseOnOff;
}

QString cPower1ModuleMeasProgram::dspGetPhaseVarStr(int phase, QString separator)
{
    QString strVarData;
    QString modeMask = m_measModeSelector.getCurrentMask();
    if(phase < modeMask.size())
        strVarData = QString("XMMODEPHASE%1%2%3").arg(phase).arg(separator, modeMask.mid(phase,1));
    return strVarData;
}

QString cPower1ModuleMeasProgram::dspGetSetPhasesVar()
{
    QStringList phaseOnOffList;
    QString modeMask = m_measModeSelector.getCurrentMask();
    for(int phase=0; phase<modeMask.size(); phase++)
        phaseOnOffList += dspGetPhaseVarStr(phase, ":");
    return phaseOnOffList.join(";");
}

void cPower1ModuleMeasProgram::dspSetParamsTiMModePhase(int tiTimeOrPeriods)
{
    QString strVarData = QString("TIPAR:%1;TISTART:0;MMODE:%2;MMODE_SUM:%3")
                             .arg(tiTimeOrPeriods)
                             .arg(m_measModeSelector.getCurrMode()->getDspSelectCode())
                             .arg(m_measModeSelector.getCurrMode()->getDspSumSelectCode());
    QString phaseVarSet = dspGetSetPhasesVar();
    if(!phaseVarSet.isEmpty())
        strVarData += ";" + phaseVarSet;
    m_dspVars.getParameters()->setVarData(strVarData);
    m_MsgNrCmdList[m_dspInterface->dspMemoryWrite(m_dspVars.getParameters())] = writeparameter;
}

void cPower1ModuleMeasProgram::handleMModeParamChange()
{
    dspSetParamsTiMModePhase(calcTiTime());
    emit m_pModule->parameterChanged();
}

void cPower1ModuleMeasProgram::handleMovingWindowIntTimeChange()
{
    m_movingwindowFilter.setIntegrationtime(getConfData()->m_fMeasIntervalTime.m_fValue);
    emit m_pModule->parameterChanged();
}

void cPower1ModuleMeasProgram::updatesForMModeChange()
{
    setActualValuesNames();
    foutParamsToDsp();
}

void cPower1ModuleMeasProgram::newPhaseList(QVariant phaseList)
{
    m_measModeSelector.tryChangeMask(phaseList.toString());
}

void cPower1ModuleMeasProgram::newQRefFrequency(QVariant frequency)
{
    getConfData()->m_qrefFrequency.m_fValue = frequency.toDouble();
    setNominalPowerForQref();
    emit m_pModule->parameterChanged();
}

void cPower1ModuleMeasProgram::newNominalFrequency(QVariant frequency)
{
    getConfData()->m_nNominalFrequency.m_nValue = frequency.toInt();
    emit m_pModule->parameterChanged();
}


void cPower1ModuleMeasProgram::updatePreScaling()
{
    foutParamsToDsp();
}

void cPower1ModuleMeasProgram::newIntegrationtime(QVariant ti)
{
    getConfData()->m_fMeasIntervalTime.m_fValue = ti.toDouble();
    if (getConfData()->m_bmovingWindow)
        handleMovingWindowIntTimeChange();
    else
        handleMModeParamChange(); // is this ever reached?
}

void cPower1ModuleMeasProgram::newIntegrationPeriod(QVariant period)
{
    // there is no moving window for period
    getConfData()->m_nMeasIntervalPeriod.m_nValue = period.toInt();
    handleMModeParamChange();
}

void cPower1ModuleMeasProgram::newMeasMode(QVariant mm)
{
    m_measModeSelector.tryChangeMode(mm.toString());
}

double cPower1ModuleMeasProgram::calcTiTime()
{
    double tiTime;
    if (getConfData()->m_sIntegrationMode == "time") {
        if (getConfData()->m_bmovingWindow)
            tiTime = getConfData()->m_fmovingwindowInterval*1000.0;
        else
            tiTime = getConfData()->m_fMeasIntervalTime.m_fValue*1000.0;
    }
    else // period (just if/else for now)
        tiTime = getConfData()->m_nMeasIntervalPeriod.m_nValue;
    return tiTime;
}

void cPower1ModuleMeasProgram::setPhaseMaskValidator(std::shared_ptr<MeasMode> mode)
{
    QStringList allPhaseMasks = VeinValidatorPhaseStringGenerator::generate(getConfData()->m_sMeasSystemList.count());
    QStringList allowedPhaseMasks;
    if(canChangePhaseMask(mode)) {
        for(auto &mask : allPhaseMasks)
            if(mode->canChangeMask(mask))
                allowedPhaseMasks.append(mask);
    }
    else
        allowedPhaseMasks.append(mode->getCurrentMask());
    m_MModePhaseSelectValidator->setValidator(allowedPhaseMasks);
    m_pModule->exportMetaData();
}

void cPower1ModuleMeasProgram::updatePhaseMaskVeinComponents(std::shared_ptr<MeasMode> mode)
{
    QString newPhaseMask = mode->getCurrentMask();
    const cMeasModeInfo modeInfo = MeasModeCatalog::getInfo(getConfData()->m_sMeasuringMode.m_sValue);
    setPhaseMaskValidator(mode);
    m_pMModePhaseSelectParameter->setValue(newPhaseMask);
    m_MModeCanChangePhaseMask->setValue(canChangePhaseMask(mode));
    m_MModeMaxMeasSysCount->setValue(mode->getMaxMeasSysCount());
    m_MModePowerDisplayName->setValue(modeInfo.getActvalName());
}

bool cPower1ModuleMeasProgram::canChangePhaseMask(std::shared_ptr<MeasMode> mode)
{
    bool disablePhase = (getConfData()->m_disablephaseselect == true);
    bool hasVarMask = mode->hasVarMask();
    bool hasMultipleMeasSystems = mode->getMeasSysCount()>1;
    return hasVarMask && hasMultipleMeasSystems && !disablePhase;
}

QString cPower1ModuleMeasProgram::getPhasePowerDescription(int measSystemNo)
{
    QString strDescription;
    if(measSystemNo < MeasPhaseCount) {
        if(measSystemNo >= getConfData()->m_sMeasSystemList.count())
            strDescription = QString("Unused in this session phase %1").arg(measSystemNo+1);
        else {
            // We found nothing useful filled at the time of call - so hack
            QStringList powerChannels = getConfData()->m_sMeasSystemList[measSystemNo].split(",");
            if(powerChannels.count() == 2) {
                ChannelRangeObserver::SystemObserverPtr observer = m_pModule->getSharedChannelRangeObserver();
                const QString name0 = observer->getChannel(powerChannels[0])->getAlias();
                const QString name1 = observer->getChannel(powerChannels[1])->getAlias();
                strDescription = QString("Actual power value %1/%2").arg(name0, name1);
            }
        }
        if(strDescription.isEmpty())
            strDescription = QString("Actual power value phase %1").arg(measSystemNo+1);
    }
    else
        strDescription = QString("Actual power value sum all phases");
    return strDescription;
}

void cPower1ModuleMeasProgram::onModeTransactionOk()
{
    std::shared_ptr<MeasMode> mode = m_measModeSelector.getCurrMode();
    QString newMeasMode = mode->getName();
    getConfData()->m_sMeasuringMode.m_sValue = newMeasMode;
    handleMModeParamChange();
    updatesForMModeChange();
    updatePhaseMaskVeinComponents(mode);
}

}
