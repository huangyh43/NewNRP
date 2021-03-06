#include "nrp_communication_controller/nrp_communication_controller.h"

#include <nlohmann/json.hpp>

using namespace nlohmann;

std::unique_ptr<NRPCommunicationController> NRPCommunicationController::_instance = nullptr;

NRPCommunicationController::~NRPCommunicationController()
{
	this->_stepController = nullptr;
}

NRPCommunicationController &NRPCommunicationController::getInstance()
{
	return *(NRPCommunicationController::_instance.get());
}

NRPCommunicationController &NRPCommunicationController::resetInstance(MPI_Comm nrpComm)
{
	// Remove old server, start new one with given server URL
	NRPCommunicationController::_instance.reset(new NRPCommunicationController(nrpComm));
	return NRPCommunicationController::getInstance();
}

void NRPCommunicationController::registerStepController(GazeboStepController *stepController)
{
	this->_stepController = stepController;
}

EngineInterface::RESULT NRPCommunicationController::initialize(const std::string &initData)
{
	ConfigStorage confDat;

	try
	{
		confDat.Data = nlohmann::json::parse(initData);
	}
	catch(std::exception &e)
	{
		const auto errMsg = std::string("Unable to parse initialization data: ") + e.what();
		std::cerr << errMsg << std::endl;
		return EngineInterface::ERROR;
	}

	GazeboConfig conf(confDat);

	double waitTime = conf.maxWorldLoadTime();
	if(conf.maxWorldLoadTime() <= 0)
		waitTime = std::numeric_limits<double>::max();

	// Wait until world plugin loads and forces a load of all other plugins
	while(this->_stepController == nullptr ? 1 : !this->_stepController->finishWorldLoading())
	{
		// Wait for 100ms before retrying
		waitTime -= 0.1;
		usleep(100*1000);

		if(waitTime <= 0)
			return EngineInterface::ERROR;
	}

	return EngineInterface::SUCCESS;
}

EngineInterface::RESULT NRPCommunicationController::shutdown(const std::string &shutdownData)
{
	return EngineInterface::SUCCESS;
}

EngineInterface::step_result_t NRPCommunicationController::runLoopStep(float timeStep)
{
	if(this->_stepController == nullptr)
	{
		const auto err = std::out_of_range("Tried to run loop while the controller has not yet been initialized");
		std::cerr << err.what() << std::endl;

		throw err;
	}

	try
	{
		// Execute loop step (Note: The _deviceLock mutex has already been set by EngineJSONServer::runLoopStepHandler, so no calls to reading/writing from/to devices is possible at this moment)
		this->_stepController->runLoopStep(static_cast<double>(timeStep));
	}
	catch(const std::exception &e)
	{
		const auto errMsg = std::string("Error during Gazebo stepping: ") + e.what();
		std::cerr << errMsg << std::endl;

		throw std::runtime_error(e.what());
	}

	return EngineInterface::SUCCESS;
}

float NRPCommunicationController::getSimTime() const
{
	if(this->_stepController == nullptr)
	{
		const auto err = std::out_of_range("Tried to run loop while the controller has not yet been initialized");
		std::cerr << err.what() << std::endl;

		throw err;
	}

	return this->_stepController->getSimTime();
}

NRPCommunicationController::NRPCommunicationController(MPI_Comm nrpComm)
    : EngineMPIServer(nrpComm)
{}
