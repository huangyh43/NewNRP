#ifndef PYTHON_DEVICE_H
#define PYTHON_DEVICE_H

#include "nrp_general_library/device_interface/device_interface.h"
#include "nrp_general_library/utils/property_template.h"

#include <boost/python.hpp>

/*!
 * \brief Create a python device class based on a given DEVICE type. Also registers shared and unique pointers
 * \tparam DEVICE Device type to create class for
 * \param class_name Name of Python class. Usually the same as C++ class name
 * \param init boost::python::init type. Defines python __init__ function
 * \return Returns boost::python::object suitable for adding into the BOOST_PYTHON_MODULE(){...} block
 */
template<DEVICE_C DEVICE>
auto python_device_class_(const std::string &class_name, auto init)
{
	namespace python = boost::python;

	python::register_ptr_to_python<typename PtrTemplates<DEVICE>::shared_ptr>();
	python::register_ptr_to_python<typename PtrTemplates<DEVICE>::const_shared_ptr>();

	return python::class_<DEVICE, python::bases<DeviceInterface> >(class_name.data(), init);
}

/*!
 * \brief Struct to create Python classes for a device with a PropertyTemplate<...> base class
 * \tparam PROPERTY_DEVICE Device type for which to create Python class
 */
template<PROPERTY_TEMPLATE_C PROPERTY_DEVICE>
struct python_property_device_class
{
	/*!
	 * \brief Create a python device class based on the PROPERTY_DEVICE type. Also makes all PropertyTemplate properties accessible via Python
	 * \param class_name Name of Python class. Usually the same as C++ class name
	 * \param init boost::python::init type. Defines python __init__ function
	 * \return Returns boost::python::object suitable for adding into the BOOST_PYTHON_MODULE(){...} block
	 */
	static auto create(const std::string &class_name, auto init)
	{
		namespace python = boost::python;
		return python_property_device_class::add_property<0>(python_device_class_<PROPERTY_DEVICE>(class_name, init));
	}

	private:

	    /*!
		 * \brief Function pointer to a the getProperty call of PROPERTY_DEVICE
		 * \tparam PROP_NUM Number of Property to retrieve
		 */
	    template<int PROP_NUM>
	    static constexpr const typename PROPERTY_DEVICE::template property_t<PROP_NUM> &(PROPERTY_DEVICE::*get_data_fcn)() const  = &PROPERTY_DEVICE::template getProperty<PROP_NUM>;

		/*!
		 * \brief Function to set a property of PROPERTY_DEVICE
		 * \tparam PROP_NUM Number of Property to set
		 * \param dev Device for which to set property
		 * \param data New property data
		 */
		template<int PROP_NUM>
		static void set_data_fcn(PROPERTY_DEVICE &dev, const typename PROPERTY_DEVICE::template property_t<PROP_NUM> &data)
		{	dev.template getProperty<PROP_NUM>() = data;	}

		/*!
		 * \brief Recursively add all properties with numbers >= CUR_PROP_NUM to device_class
		 * \tparam CUR_PROP_NUM Number of property to add to python class
		 * \param device_class Python class. Property with number CUR_PROP_NUM will be made accessible from Python
		 * \return Returns modifed device_class
		 */
		template<int CUR_PROP_NUM>
		static auto add_property(auto &&device_class)
		{
			// Add property to python class
			namespace python = boost::python;

			device_class.add_property(PROPERTY_DEVICE::template getName<CUR_PROP_NUM>().data(),
			                          python::make_function(get_data_fcn<CUR_PROP_NUM>, python::return_internal_reference<>()),
			                          &set_data_fcn<CUR_PROP_NUM>);

			// If additional properties are available, add them as well
			if constexpr (CUR_PROP_NUM < PROPERTY_DEVICE::NumProperties-1)
			{	return add_property<CUR_PROP_NUM+1>(std::move(device_class));	}
			else
			{	return std::move(device_class);	}
		}
};

#endif // PYTHON_DEVICE_H
