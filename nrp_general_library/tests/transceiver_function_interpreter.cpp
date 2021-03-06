#include <gtest/gtest.h>

#include "nrp_general_library/config_headers/nrp_cmake_constants.h"
#include "tests/test_transceiver_function_interpreter.h"
#include "tests/test_env_cmake.h"

using namespace boost;

TEST(TransceiverFunctionInterpreterTest, TestSimplePythonFcn)
{
	Py_Initialize();
	python::object main(python::import("__main__"));
	python::dict globals(main.attr("__dict__"));
	try
	{
		python::object simpleFcn(python::import("simple_fcn"));
		python::incref(simpleFcn.ptr());
		globals.update(simpleFcn.attr("__dict__"));
		globals["simple_fcn"]();
	}
	catch(const boost::python::error_already_set &)
	{
		PyErr_Print();
		PyErr_Clear();

		std::cout.flush();

		throw std::invalid_argument("Python Exception");
	}

	TransceiverFunctionInterpreterSharedPtr interpreter(new TransceiverFunctionInterpreter(globals));

	TransceiverDeviceInterface::setTFInterpreter(interpreter.get());

	// Load and execute simple python function
	const std::string tfName = "testTF";
	TestSimpleTransceiverDevice::shared_ptr tfDevice(new TestSimpleTransceiverDevice(globals["simple_fcn"]));
	interpreter->loadTransceiverFunction(tfName, tfDevice);

	// Test execution result
	boost::python::object res(interpreter->runSingleTransceiverFunction(tfName));
	python::incref(res.ptr());
	ASSERT_EQ(boost::python::extract<int>(res), 3);

	// Test invalid TF name
	ASSERT_THROW(interpreter->runSingleTransceiverFunction("invalidTFName"), std::invalid_argument);

	TransceiverDeviceInterface::setTFInterpreter(nullptr);
}

TEST(TransceiverFunctionInterpreterTest, TestTransceiverFcnDevices)
{
	Py_Initialize();
	python::object main(python::import("__main__"));
	python::object nrpModule(python::import(PYTHON_MODULE_NAME_STR));
	python::object testModule(python::import(TEST_PYTHON_MODULE_NAME_STR));

	python::dict globals(main.attr("__dict__"));
	globals.update(nrpModule.attr("__dict__"));
	globals.update(testModule.attr("__dict__"));

	TransceiverFunctionInterpreterSharedPtr interpreter(new TransceiverFunctionInterpreter(globals));

	TransceiverDeviceInterface::setTFInterpreter(interpreter.get());

	std::shared_ptr<TestOutputDevice> dev(new TestOutputDevice(TestOutputDevice::ID()));
	interpreter->setEngineOutputDeviceData(dev->engineName(), {dev});

	// Load and execute simple python function
	const std::string tfName = "testTF";
	TestTransceiverDevice::shared_ptr tfDevice(new TestTransceiverDevice());
	interpreter->loadTransceiverFunction(tfName, tfDevice);

	const auto &reqIDs = interpreter->updateRequestedDeviceIDs();
	ASSERT_EQ(reqIDs.size(), 1);
	ASSERT_EQ(*(reqIDs.begin()), TestOutputDevice::ID());

	// Test execution result
	boost::python::list res;
	res = static_cast<boost::python::list>(interpreter->runSingleTransceiverFunction(tfName));

	ASSERT_EQ(boost::python::len(res), 1);

	const TestInputDevice &inDevice = boost::python::extract<TestInputDevice>(res[0]);
	ASSERT_EQ(inDevice.id(), TestInputDevice::ID());
	ASSERT_EQ(dev->TestValue, std::stoi(inDevice.TestValue));

	TransceiverDeviceInterface::setTFInterpreter(nullptr);
}

TEST(TransceiverFunctionInterpreterTest, TestTransceiverFunction)
{
	Py_Initialize();
	python::object main(python::import("__main__"));
	python::object nrpModule(python::import(PYTHON_MODULE_NAME_STR));
	python::object testModule(python::import(TEST_PYTHON_MODULE_NAME_STR));
//	python::object transceiverFcnModule(python::import("transceiver_function"));

	python::dict globals(main.attr("__dict__"));
	globals.update(nrpModule.attr("__dict__"));
	globals.update(testModule.attr("__dict__"));
//	globals.update(transceiverFcnModule.attr("__dict__"));

	TransceiverFunctionInterpreterSharedPtr interpreter(new TransceiverFunctionInterpreter(globals));

	TransceiverDeviceInterface::setTFInterpreter(interpreter.get());

	const std::string tfName = "testTF";

	TransceiverFunctionConfig tfCfg;
	tfCfg.setName(tfName);
	tfCfg.setFileName(TEST_TRANSCEIVER_FCN_FILE_NAME);
	tfCfg.setIsActive(true);

	std::shared_ptr<TestOutputDevice> dev(new TestOutputDevice(TestOutputDevice::ID()));
	dev->TestValue = 4;
	interpreter->setEngineOutputDeviceData(dev->engineName(), {dev});

	// Load and execute simple python function
	interpreter->loadTransceiverFunction(tfCfg);

	const auto &reqIDs = interpreter->updateRequestedDeviceIDs();
	ASSERT_EQ(reqIDs.size(), 1);
	ASSERT_EQ(*(reqIDs.begin()), TestOutputDevice::ID());

	// Test execution result
	boost::python::list res(interpreter->runSingleTransceiverFunction(tfName));
	ASSERT_EQ(boost::python::len(res), 1);

	const TestInputDevice &inDevice = boost::python::extract<TestInputDevice>(res[0]);
	ASSERT_EQ(inDevice.id(), TestInputDevice::ID());
	ASSERT_EQ(dev->TestValue, std::stoi(inDevice.TestValue));

	TransceiverDeviceInterface::setTFInterpreter(nullptr);
}

