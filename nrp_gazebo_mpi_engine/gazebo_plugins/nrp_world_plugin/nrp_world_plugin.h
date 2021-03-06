#ifndef NRP_WORLD_PLUGIN_H
#define NRP_WORLD_PLUGIN_H

#include "nrp_gazebo_mpi_engine/engine_server/gazebo_step_controller.h"

#include <gazebo/gazebo.hh>
#include <gazebo/physics/JointController.hh>
#include <gazebo/physics/Joint.hh>

namespace gazebo
{
	/*!
	 * \brief Interface for a single joint
	 */
	class NRPWorldPlugin
	        : public GazeboStepController,
	          public WorldPlugin
	{
		public:
			virtual ~NRPWorldPlugin() override = default;

			void Load(physics::WorldPtr world, sdf::ElementPtr sdf) override;
			void Reset() override;

			double runLoopStep(double timeStep) override;
			float getSimTime() const override;

			bool finishWorldLoading() override;

		private:
			/*!
			 * \brief Lock to ensure only one loop is being executed
			 */
			std::mutex _lockLoop;

			physics::WorldPtr _world;
			sdf::ElementPtr _worldSDF;

			/*!
			 * \brief Start running the sim.
			 * \param numIterations Number of iterations to run
			 */
			void startLoop(unsigned int numIterations);
	};

	GZ_REGISTER_WORLD_PLUGIN(NRPWorldPlugin)
}

#endif // NRP_WORLD_PLUGIN
