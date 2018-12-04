/*
 * Copyright 2017 Autoware Foundation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ros/ros.h>
#include <gtest/gtest.h>

#include "state_machine_lib/state.hpp"

class TestSuite: public ::testing::Test {
public:
	TestSuite(){}
	~TestSuite(){}
};

TEST(TestSuite, CheckStateConstructor){

	std::string state_name = "TestState";
	uint64_t state_id = 0;
	state_machine::State state(state_name, state_id);

	ASSERT_EQ(state.getStateID(),state_id) << "_state_id should be " << state_id;
	ASSERT_STREQ(state.getStateName().c_str(), state_name.c_str()) << "state_name should be " << state_name;
	ASSERT_TRUE(state.getChild() == NULL) << "child_state_ pointer should be NULL";
	ASSERT_TRUE(state.getParent() == NULL) << "parent_state_ pointer should be NULL";
	ASSERT_STREQ(state.getEnteredKey().c_str(), "") << "entered_key should be " << "";


	std::ostringstream oss;
	std::streambuf* p_cout_streambuf = std::cout.rdbuf();
	std::cout.rdbuf(oss.rdbuf());

	state.showStateName();

	std::cout.rdbuf(p_cout_streambuf); // restore

	// Test oss content...
	ASSERT_TRUE(oss && oss.str() == state_name + "-") << "Should show" << state_name << "-";

}

TEST(TestSuite, TestParentChild){

	std::string state_name;
	uint64_t state_id;

	state_name = "Start";
	state_id = 0;
	state_machine::State *first_state = new state_machine::State(state_name, state_id);
	std::shared_ptr<state_machine::State> first_state_ptr(first_state);

	state_name = "Init";
	state_id = 1;
	state_machine::State *second_state = new state_machine::State(state_name, state_id);
	std::shared_ptr<state_machine::State> second_state_ptr(second_state);

	state_name = "Final";
	state_id = 2;
	state_machine::State *third_state = new state_machine::State(state_name, state_id);
	std::shared_ptr<state_machine::State> third_state_ptr(third_state);

	// Set parent
	second_state_ptr->setParent(first_state_ptr);
	third_state_ptr->setParent(second_state_ptr);

	ASSERT_TRUE(first_state_ptr->getParent() == NULL) << "Parent should be " << NULL;
	ASSERT_TRUE(second_state_ptr->getParent() == first_state_ptr) << "Parent should be " << first_state_ptr;
	ASSERT_TRUE(third_state_ptr->getParent() == second_state_ptr) << "Parent should be " << second_state_ptr;

	// Set child
	first_state_ptr->setChild(second_state_ptr);
	second_state_ptr->setChild(third_state_ptr);

	ASSERT_TRUE(first_state_ptr->getChild() == second_state_ptr) << "Child should be " << second_state_ptr;
	ASSERT_TRUE(second_state_ptr->getChild() == third_state_ptr) << "Child should be " << third_state_ptr;
	ASSERT_TRUE(third_state_ptr->getChild() == NULL) << "Child should be " << NULL;
}

void auxFunc(const std::string&){
	std::cout << "Test output";
};

TEST(TestSuite, SetCallbacks){

	std::string state_name;
	uint64_t state_id;

	state_name = "Start";
	state_id = 0;
	state_machine::State *first_state = new state_machine::State(state_name, state_id);
	std::shared_ptr<state_machine::State> first_state_ptr(first_state);

	state_name = "Init";
	state_id = 1;
	state_machine::State *second_state = new state_machine::State(state_name, state_id);
	std::shared_ptr<state_machine::State> second_state_ptr(second_state);

	// Set child
	first_state_ptr->setChild(second_state_ptr);

	// Set callbacks
	std::function<void(const std::string&)> _f = &auxFunc;
	first_state_ptr->setCallbackEntry(_f);
	first_state_ptr->setCallbackExit(_f);
	first_state_ptr->setCallbackUpdate(_f);

	std::ostringstream oss;
	std::streambuf* p_cout_streambuf = std::cout.rdbuf();
	std::cout.rdbuf(oss.rdbuf());

	first_state_ptr->onEntry();
	std::cout.rdbuf(p_cout_streambuf); // restore
	ASSERT_TRUE(oss && oss.str() == "Test output") << "onEntry should show Test output";

	first_state_ptr->onUpdate();
	std::cout.rdbuf(p_cout_streambuf); // restore
	ASSERT_TRUE(oss && oss.str() == "Test output") << "onUpdate should show Test output";

	first_state_ptr->onExit();
	std::cout.rdbuf(p_cout_streambuf); // restore
	ASSERT_TRUE(oss && oss.str() == "Test output") << "onExit should show Test output";
}

TEST(TestSuite, SetKey){

	std::string state_name;
	uint64_t state_id;

	state_name = "Start";
	state_id = 0;
	state_machine::State *first_state = new state_machine::State(state_name, state_id);
	std::shared_ptr<state_machine::State> first_state_ptr(first_state);

	ASSERT_STREQ(first_state_ptr->getEnteredKey().c_str(), "") << "entered_key should be " << "";
	std::string key = "newKey";
	first_state_ptr->setEnteredKey(key);
	ASSERT_STREQ(first_state_ptr->getEnteredKey().c_str(), key.c_str()) << "entered_key should be " << key;
}

TEST(TestSuite, TestTransitionMap){

	std::string state_name;
	uint64_t state_id;
	std::string key;
	uint64_t value;
	std::map<std::string, uint64_t> transition_map;

	state_name = "Start";
	state_id = 0;
	state_machine::State *first_state = new state_machine::State(state_name, state_id);
	std::shared_ptr<state_machine::State> first_state_ptr(first_state);

	transition_map = first_state_ptr->getTransitionMap();
	ASSERT_EQ(transition_map[""], 0) << "Transition value should be" << 0;

	key = "newTransition";
	value = 0;

	first_state_ptr->addTransition(key, value);
	ASSERT_EQ(first_state_ptr->getTansitionVal(key), value) << "Transition value for key : " << key << " should be " << value;

	transition_map = first_state_ptr->getTransitionMap();
	ASSERT_EQ(transition_map[key], value) << "Transition value should be" << value;
}

