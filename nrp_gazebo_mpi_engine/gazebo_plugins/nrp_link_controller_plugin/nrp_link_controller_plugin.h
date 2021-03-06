#ifndef NRP_LINK_CONTROLLER_PLUGIN_H
#define NRP_LINK_CONTROLLER_PLUGIN_H

#include "nrp_general_library/engine_interfaces/engine_mpi_interface/engine_server/engine_mpi_device_controller.h"
#include "nrp_gazebo_mpi_engine/devices/physics_link.h"

#include <gazebo/gazebo.hh>

namespace gazebo
{
	/*!
	 * \brief Interface for links
	 */
	class LinkDeviceController
	        : public EngineMPIDeviceController<PhysicsLink>
	{
		public:
			LinkDeviceController(const std::string &linkName, const physics::LinkPtr &link);
			virtual ~LinkDeviceController() override;

			MPIPropertyData getDeviceOutput() override;
			EngineInterface::RESULT handleDeviceInput(PhysicsLink &data) override;

		private:
			/*!
			 * \brief Link Data
			 */
			PhysicsLink _data;

			/*!
			 * \brief Pointer to link
			 */
			physics::LinkPtr _link;
	};

	class NRPLinkControllerPlugin
	        : public gazebo::ModelPlugin
	{
		public:
			virtual ~NRPLinkControllerPlugin();

			virtual void Load(physics::ModelPtr model, sdf::ElementPtr sdf);

		private:

			std::list<LinkDeviceController> _linkInterfaces;
	};

	GZ_REGISTER_MODEL_PLUGIN(NRPLinkControllerPlugin)
}

#endif
