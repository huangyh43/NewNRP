#ifndef ENGINE_JSON_NRP_CLIENT_H
#define ENGINE_JSON_NRP_CLIENT_H

#include "nrp_general_library/device_interface/device_interface.h"
#include "nrp_general_library/engine_interfaces/engine_interface.h"
#include "nrp_general_library/engine_interfaces/engine_json_interface/config/engine_json_config.h"
#include "nrp_general_library/engine_interfaces/engine_json_interface/device_interfaces/json_device_conversion_mechanism.h"
#include "nrp_general_library/engine_interfaces/engine_json_interface/nrp_client/engine_json_registration_server.h"
#include "nrp_general_library/utils/restclient_setup.h"

#include <nlohmann/json.hpp>
#include <list>

#include <chrono>
#include <future>

#include <iostream>
#include <restclient-cpp/restclient.h>

/*!
 *  \brief NRP - Gazebo Communicator on the NRP side. Converts DeviceInterface classes from/to JSON objects
 *  \tparam ENGINE_INTERFACE Class derived from GeneralInterface. Currently either PhysicsInterface or BrainInterface
 *  \tparam DEVICES Classes derived from DeviceInterface that should be communicated to/from the engine. Each of these classes must be convertible via a DeviceConversionMechanism.
 */
template<class ENGINE, ENGINE_CONFIG_C ENGINE_CONFIG, DEVICE_C ...DEVICES>
class EngineJSONNRPClient
        : public Engine<ENGINE, ENGINE_CONFIG>
{
	public:
		/*!
		 * \brief Constructor
		 * \param config Engine Config
		 * \param launcher Process launcher
		 */
		EngineJSONNRPClient(EngineConfigConst::config_storage_t &config, ProcessLauncherInterface::unique_ptr &&launcher)
		    : Engine<ENGINE, ENGINE_CONFIG>(config, std::move(launcher)),
		      _serverAddress(this->engineConfig()->engineServerAddress())
		{	RestClientSetup::ensureInstance();	}

		/*!
		 * \brief Constructor
		 * \param serverAddress Server Address to send requests to
		 * \param config Engine Config
		 * \param launcher Process launcher
		 */
		EngineJSONNRPClient(const std::string &serverAddress, EngineConfigConst::config_storage_t &config, ProcessLauncherInterface::unique_ptr &&launcher)
		    : Engine<ENGINE, ENGINE_CONFIG>(config, std::move(launcher)),
		      _serverAddress(serverAddress)
		{
			this->engineConfig()->engineServerAddress() = this->_serverAddress;
			RestClientSetup::ensureInstance();
		}

		virtual ~EngineJSONNRPClient() override = default;

		virtual pid_t launchEngine() override
		{
			// Launch engine process
			auto enginePID = this->EngineInterface::launchEngine();

			// Wait for engine to register itself
			if(!this->engineConfig()->engineRegistrationServerAddress().empty())
			{
				const auto serverAddr = this->waitForRegistration(20, 1);
				if(serverAddr.empty())
				{
					const auto errMsg = "Error while waiting for engine \"" + this->engineName() + "\" to register its address. Did not receive a reply";
					std::cerr << errMsg << std::endl;

					throw std::runtime_error(errMsg);
				}

				this->engineConfig()->engineServerAddress() = serverAddr;
				this->_serverAddress = serverAddr;
			}

			return enginePID;
		}

		virtual typename EngineInterface::device_outputs_t getOutputDevices(const typename EngineInterface::device_identifiers_t &deviceIdentifiers) override
		{
			nlohmann::json request;
			for(const auto &devID : deviceIdentifiers)
			{
				if(this->engineName().compare(devID.EngineName) == 0)
					request.update(this->_dcm.serializeID(devID));
			}

			// Post request to Engine JSON server
			const auto resp(EngineJSONNRPClient::sendRequest(this->_serverAddress + "/" + EngineJSONConfigConst::EngineServerGetDevicesRoute.data(),
			                                                 EngineJSONConfigConst::EngineServerContentType.data(), request.dump(),
			                                                 "Engine server \"" + this->engineName() + "\" failed during device retrieval"));

			return this->getDeviceInterfacesFromJSON(resp);
		}

		virtual typename EngineInterface::RESULT handleInputDevices(const typename EngineInterface::device_inputs_t &inputDevices) override
		{
			// Convert devices to JSON format
			nlohmann::json request;
			for(const auto &curDevice : inputDevices)
			{
				if(curDevice->engineName().compare(this->engineName()) == 0)
					request.update(this->getJSONFromSingleDeviceInterface<DEVICES...>(*curDevice));
			}

			// Send updated devices to Engine JSON server
			EngineJSONNRPClient::sendRequest(this->_serverAddress + "/" + EngineJSONConfigConst::EngineServerSetDevicesRoute.data(),
			                                 EngineJSONConfigConst::EngineServerContentType.data(), request.dump(),
			                                 "Engine server \"" + this->engineName() + "\" failed during device handling");

			// TODO: Check if engine has processed all sent devices
			return EngineInterface::SUCCESS;
		}

		float getEngineTime() const override
		{	return this->_engineTime;	}

		virtual typename EngineInterface::step_result_t runLoopStep(float timeStep) override
		{
			this->_loopStepThread = std::async(std::launch::async, std::bind(&EngineJSONNRPClient::loopFcn, this, timeStep));
			return EngineInterface::SUCCESS;
		}

		virtual typename EngineInterface::RESULT waitForStepCompletion(float timeOut) override
		{
			// If thread state is invalid, loop thread has completed and waitForStepCompletion was called once before
			if(!this->_loopStepThread.valid())
				return EngineInterface::SUCCESS;

			// Wait until timeOut has passed
			if(timeOut > 0)
			{
				if(this->_loopStepThread.wait_for(std::chrono::duration<double>(timeOut)) != std::future_status::ready)
					return EngineInterface::ERROR;
			}
			else
				this->_loopStepThread.wait();

			this->_engineTime = this->_loopStepThread.get();
			return EngineInterface::SUCCESS;
		}

	protected:
		/*!
		 * \brief Send an initialization command
		 * \param data Data that should be passed to the engine
		 * \return Returns init data from engine
		 */
		nlohmann::json sendInitCommand(const nlohmann::json &data)
		{
			// Post init request to Engine JSON server
			return sendRequest(this->_serverAddress + "/" + EngineJSONConfigConst::EngineServerInitializeRoute.data(),
			                   EngineJSONConfigConst::EngineServerContentType.data(), data.dump(),
			                   "Engine server \"" + this->engineName() + "\" failed during initialization");
		}

		/*!
		 * \brief Send a shutdown command
		 * \param data Data that should be passed to the engine
		 * \return Returns init data from engine
		 */
		nlohmann::json sendShutdownCommand(const nlohmann::json &data)
		{
			// Post init request to Engine JSON server
			return sendRequest(this->_serverAddress + "/" + EngineJSONConfigConst::EngineServerShutdownRoute.data(),
			                   EngineJSONConfigConst::EngineServerContentType.data(), data.dump(),
			                   "Engine server \"" + this->engineName() + "\" failed during shutdown");
		}

		/*!
		 * \brief Wait for the engine registration server to receive a call from the engine
		 * \param numTries Number of times to check the registration server for an address
		 * \param waitTime Wait time (in seconds) between checks
		 * \return Returns engine server address if present, empty string otherwise
		 */
		std::string waitForRegistration(unsigned int numTries, unsigned int waitTime) const
		{
			auto *pRegistrationServer = EngineJSONRegistrationServer::getInstance();
			if(pRegistrationServer == nullptr)
				pRegistrationServer = EngineJSONRegistrationServer::resetInstance(this->engineConfig()->engineRegistrationServerAddress());

			if(!pRegistrationServer->isRunning())
				pRegistrationServer->startServerAsync();

			// Try to retrieve engine address
			auto engineAddr = pRegistrationServer->requestEngine(this->engineName());

			while(engineAddr.empty() && numTries > 0)
			{
				// Continue to wait for engine address for 20s
				sleep(waitTime);
				engineAddr = pRegistrationServer->retrieveEngineAddress(this->engineName());
				--numTries;
			}

			// Close server if no additional clients are waiting for their engines to register
			if(pRegistrationServer->getNumWaitingEngines() == 0)
				EngineJSONRegistrationServer::clearInstance();

			return engineAddr;
		}

		using dcm_t = DeviceConversionMechanism<nlohmann::json, nlohmann::json::const_iterator, DEVICES...>;
		dcm_t _dcm;

	private:
		/*!
		 * \brief Future of thread running a single loop. Used by runLoopStep and waitForStepCompletion to execute the thread
		 */
		std::future<float> _loopStepThread;

		/*!
		 * \brief Server Address to send requests to
		 */
		std::string _serverAddress;

		/*!
		 * \brief Engine Time
		 */
		float _engineTime = 0;

		/*!
		 * \brief Send a request to the Server
		 * \param serverName Name of the server
		 * \param contentType Content Type
		 * \param request Body of request
		 * \param exceptionMessage Message to put into exception output
		 * \return Returns body of response, parsed as JSON
		 */
		static nlohmann::json sendRequest(const std::string &serverName, const std::string &contentType, const std::string &request, const std::string_view &exceptionMessage)
		{
			// Post request to Engine JSON server
			try
			{
				auto resp = RestClient::post(serverName, contentType, request);
				if(resp.code != 200)
				{
					throw std::domain_error(exceptionMessage.data());
				}

				return nlohmann::json::parse(resp.body);
			}
			catch (const std::exception &e)
			{
				std::cerr << "Communication with engine server failed\n";
				std::cerr << e.what() << std::endl;

				throw;
			}
		}

		/*!
		 * \brief Thread function that executes the loop and waits for a result from the engine
		 * \param timeStep Time (in seconds) to execute the engine
		 * \return Returns current time of engine
		 */
		float loopFcn(float timeStep)
		{
			nlohmann::json request;
			request[EngineJSONConfigConst::EngineTimeStepName.data()] = timeStep;

			// Post run loop request to Engine JSON server
			nlohmann::json resp(EngineJSONNRPClient::sendRequest(this->_serverAddress + "/" + EngineJSONConfigConst::EngineServerRunLoopStepRoute.data(),
			                                                     EngineJSONConfigConst::EngineServerContentType.data(), request.dump(),
			                                                     "Engine Server failed during loop execution"));

			// Get engine time from response
			float engineTime;
			try
			{
				engineTime = resp[EngineJSONConfigConst::EngineTimeName.data()];
			}
			catch (const std::exception &e)
			{
				std::cerr << "Error while parsing the return value of the run_step of " + this->engineName() << std::endl;
				std::cerr << e.what() << std::endl;

				throw;
			}

			if(engineTime < 0)
			{
				const auto errMsg = "Error during execution of engine " + this->engineName();
				std::cerr << errMsg << std::endl;

				throw std::runtime_error(errMsg);
			}

			return engineTime;
		}

		/*!
		 * \brief Extracts devices from given JSON
		 * \param devices JSON data of devices
		 * \return Returns list of devices
		 */
		typename EngineInterface::device_outputs_t getDeviceInterfacesFromJSON(const nlohmann::json &devices) const
		{
			typename EngineInterface::device_outputs_t interfaces;
			interfaces.reserve(devices.size());

			for(auto curDeviceIterator = devices.begin(); curDeviceIterator != devices.end(); ++curDeviceIterator)
			{
				if(curDeviceIterator.value().empty())
				{
					// TODO: Print warning that device was requested but not found
					continue;
				}

				try
				{
					auto deviceID = this->_dcm.getID(curDeviceIterator);
					deviceID.EngineName = this->engineName();
					interfaces.push_back(this->getSingleDeviceInterfaceFromJSON<DEVICES...>(curDeviceIterator, deviceID));
				}
				catch(const std::exception &e)
				{
					// TODO: Handle json device parsing error
					std::cerr << e.what();

					throw;
				}
			}

			return interfaces;
		}

		/*!
		 * \brief Go through given DEVICES and try to create a DeviceInterface from the JSON object
		 * \tparam DEVICE DeviceInterface Class to check
		 * \tparam REMAINING_DEVICES Remaining DeviceInterface Classes to check
		 * \param deviceData Device data as JSON object
		 * \param deviceID ID of device
		 * \return Returns pointer to created device
		 */
		template<class DEVICE, class ...REMAINING_DEVICES>
		inline DeviceInterfaceConstSharedPtr getSingleDeviceInterfaceFromJSON(const nlohmann::json::const_iterator &deviceData, const DeviceIdentifier &deviceID) const
		{
			// Only check DEVICE classes with an existing conversion function
			if constexpr (dcm_t::template IsDeserializable<DEVICE>)
			{
				if(DEVICE::TypeName.compare(deviceID.Type) == 0)
				{
					DeviceInterfaceSharedPtr newDevice(new DEVICE(this->_dcm.template deserialize<DEVICE>(deviceData)));
					newDevice->setEngineName(this->engineName());

					return newDevice;
				}
			}

			// If device classess are left to check, go through them. If all device classes have been checked without proper result, throw an error
			if constexpr (sizeof...(REMAINING_DEVICES) > 0)
			    return this->getSingleDeviceInterfaceFromJSON<REMAINING_DEVICES...>(deviceData, deviceID);
			else
			    throw std::logic_error("Could not process given device of type " + deviceID.Type);
		}

		/*!
		 * \brief Go through given DEVICES and try to create a JSON object from the device interface
		 * \tparam DEVICE DeviceInterface Class to check
		 * \tparam REMAINING_DEVICES Remaining DeviceInterface Classes to check
		 * \param device Device data
		 * \return Returns JSON object with device data
		 */
		template<class DEVICE, class ...REMAINING_DEVICES>
		inline nlohmann::json getJSONFromSingleDeviceInterface(const DeviceInterface &device) const
		{
			// Only check DEVICE classes with an existing conversion function
			if constexpr (dcm_t::template IsSerializable<DEVICE>)
			{
				if(DEVICE::TypeName.compare(device.type()) == 0)
					return this->_dcm.template serialize<DEVICE>(dynamic_cast<const DEVICE&>(device));
			}

			// If device classess are left to check, go through them. If all device classes have been checked without proper result, throw an error
			if constexpr (sizeof...(REMAINING_DEVICES) > 0)
			{	return this->getJSONFromSingleDeviceInterface<REMAINING_DEVICES...>(device);	}
			else
			{	throw std::logic_error("Could not process given device of type " + device.type());	}
		}
};


#endif //ENGINE_JSON_NRP_CLIENT_H
