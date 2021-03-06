#include "nrp_camera_controller_plugin/nrp_camera_controller_plugin.h"

#include "nrp_communication_controller/nrp_communication_controller.h"

gazebo::CameraDeviceController::CameraDeviceController(const std::string &devName, const rendering::CameraPtr &camera, const sensors::SensorPtr &parent)
    : EngineJSONDeviceController(DeviceIdentifier(devName, PhysicsCamera::TypeName.data(), "")),
      _camera(camera),
      _parentSensor(parent),
      _data(camera->ScopedName())
{}

gazebo::CameraDeviceController::~CameraDeviceController() = default;

nlohmann::json gazebo::CameraDeviceController::getDeviceInformation(const nlohmann::json::const_iterator&)
{
	// Render image
//	try{this->_camera->Render(false);}
//	catch(std::exception &e)
//	{
//		std::cerr << "Error during camera rendering:\n" << e.what() << "\n";
//		std::cerr << "Last recorded image at: " << this->_camera->LastRenderWallTime() << "\n";
//		//throw;

//		return JSONPropertySerializer<PhysicsCamera>::serializeProperties(this->_data, nlohmann::json());
//	}

	// Data updated via updateCamData()
	return JSONPropertySerializer<PhysicsCamera>::serializeProperties(this->_data, nlohmann::json());

	// Save image data
//	const unsigned char *img_data = this->_camera->ImageData();
//	const unsigned char *const img_data_end = img_data + this->_camera->ImageByteSize();
//	auto img = nlohmann::json::array();
//	for (; img_data < img_data_end; ++img_data)
//	{
//		img.push_back(*img_data);
//	}

//	retVal[PhysicsCamera::ImageData.m_data] = std::move(img);
}

nlohmann::json gazebo::CameraDeviceController::handleDeviceData(const nlohmann::json &data)
{
	return nlohmann::json();
}

void gazebo::CameraDeviceController::updateCamData(const unsigned char *image, unsigned int width, unsigned int height, unsigned int depth)
{
	const common::Time sensorUpdateTime = this->_parentSensor->LastMeasurementTime();

	if(sensorUpdateTime > this->_lastSensorUpdateTime)
	{
		std::cout << "Updating camera data\n";
		this->_lastSensorUpdateTime = sensorUpdateTime;

		// Set headers
		this->_data.setImageHeight(height);
		this->_data.setImageWidth(width);
		this->_data.setImagePixelSize(depth);

		const auto imageSize = width*height*depth;
		this->_data.imageData().resize(imageSize);
		memcpy(this->_data.imageData().data(), image, imageSize);
	}
}

gazebo::NRPCameraController::~NRPCameraController() = default;

void gazebo::NRPCameraController::Load(gazebo::sensors::SensorPtr sensor, sdf::ElementPtr sdf)
{
	// Load camera plugin
	this->CameraPlugin::Load(sensor, sdf);

	const auto devName = NRPCommunicationController::createDeviceName(*this, sensor->Name());
	std::cout << "NRPCameraController: Registering new controller \"" << devName << "\"\n";

	// Create camera device and register it
	this->_cameraInterface.reset(new CameraDeviceController(devName, this->camera, sensor));
	NRPCommunicationController::getInstance().registerDevice(devName, this->_cameraInterface.get());
}

void gazebo::NRPCameraController::OnNewFrame(const unsigned char *image, unsigned int width, unsigned int height, unsigned int depth, const std::string &)
{	this->_cameraInterface->updateCamData(image, width, height, depth);	}
