/*
 *   test_toolkitAPI_hotstart.cpp
 *
 *   Created: 07/29/2019
 *   Updated: 06/11/2023
 *   Author: Bryant E. McDonnell and Jeff Sadler
 *
 *   Unit testing mechanics for the hotstart API using Boost Test.
 */

#include <boost/test/unit_test.hpp>
#include "test_solver.hpp"
#include <fstream>
#include <iterator>

#define HOTSTART_SWMM_SAVE_NORMAL "hotstart/INFILE_Simulation1.hsf"
#define HOTSTART_API_SAVE_DURING_SIM1 "hotstart/swmm_api_test_during_Simulation1.hsf"
#define HOTSTART_API_SAVE_AFTER_SIM1 "hotstart/swmm_api_test_after_Simulation1.hsf"
#define HOTSTART_SWMM_SAVE_SIM2 "hotstart/INFILE_Simulation2.hsf"

#define ERR_NONE 0

BOOST_AUTO_TEST_SUITE(test_save_hotstart)

// Testing Run Simulation and Generate Hot Start File using Model (as normal)
// and save a hot start file using the API swmm_saveHotstart()
//
// TEST #1 - Making and Comparing Hot Start Files.
// In this test 2 simulations will be performed.  One simulation will
// create hot start files at various time and the second simulation will
// end at the
//
// Start Simulation 1
// This simulation will produce 3 hot start files:
//  1. API hotstart File at 1056 Steps into the model ~= 1998-1-1 2:59:58
//     (HSF Name: HOTSTART_API_SAVE_DURING_SIM1)
//  2. API created hotstart file at the end of the run
//     (HSF Name: HOTSTART_API_SAVE_AFTER_SIM1)
//  3. Normal SWMM [FILES] Generated HSF
//     (HSF Name: HOTSTART_SWMM_SAVE_NORMAL)
//
//  Start Simulation 2
//  This simulation will end at 1056 Steps into a simulation.
//     (HSF Name: HOTSTART_SWMM_SAVE_SIM2
//
//  The follwing comparisons will be made from these artifacts
//  1.  HOTSTART_SWMM_SAVE_NORMAL Should == HOTSTART_API_SAVE
//  2.  HOTSTART_API_SAVE_DURING_SIM1 Should == HOTSTART_SWMM_SAVE_SIM2

BOOST_AUTO_TEST_CASE(save_hotstart_file){
    int error, step_ind;
    int index;
    int number_of_nodes;
    int y, m, d, h, mn, s;
    double elapsedTime = 0.0;
    double read_val, set_val;

    // Start Simulation 1
    swmm_open((char *)"hotstart/Simulation1.inp",
              (char *)"hotstart/Simulation1.rpt",
              (char *)"hotstart/Simulation1.out");
    swmm_start(1);
    step_ind = 0;
    do
    {
        error = swmm_step(&elapsedTime);
        step_ind += 1;
        if (step_ind == 1056)
        {
          swmm_saveHotstart((char *) HOTSTART_API_SAVE_DURING_SIM1);
        }
    }while (elapsedTime != 0 && !error);
    BOOST_CHECK_EQUAL(ERR_NONE, error);
    // Save New Hotstart File at the End of the Simulation
    error = swmm_saveHotstart((char *) HOTSTART_API_SAVE_AFTER_SIM1);
    BOOST_CHECK_EQUAL(ERR_NONE, error);
    swmm_end();
    swmm_report();
    swmm_close();

    // Start Simulation 2
    swmm_open((char *)"hotstart/Simulation2.inp",
              (char *)"hotstart/Simulation2.rpt",
              (char *)"hotstart/Simulation2.out");
    swmm_start(0);
    step_ind = 0;
    elapsedTime = 0.0;
    do
    {
        error = swmm_step(&elapsedTime);
        step_ind += 1;
    }while (step_ind < 1056 && !error);
    BOOST_CHECK_EQUAL(ERR_NONE, error);
    swmm_end();
    swmm_close();

    std::ifstream ifs1(HOTSTART_SWMM_SAVE_NORMAL);
    std::ifstream ifs2(HOTSTART_API_SAVE_AFTER_SIM1);
    std::istream_iterator<char> b1(ifs1), e1;
    std::istream_iterator<char> b2(ifs2), e2;
    //iterate over the two hotstart files and check all of them
    BOOST_CHECK_EQUAL_COLLECTIONS(b1, e1, b2, e2);

    // Diff the three hot start files that come from the model.
    std::ifstream ifsbench_1998(HOTSTART_SWMM_SAVE_SIM2);
    std::ifstream ifs1_1998(HOTSTART_API_SAVE_DURING_SIM1);
    std::istream_iterator<char> bench_1998(ifsbench_1998), ebench_1998;
    std::istream_iterator<char> b_1998(ifs1_1998), e_1998;
    // iterate over hotstart files and check all of them
    BOOST_CHECK_EQUAL_COLLECTIONS(bench_1998, ebench_1998, b_1998, e_1998);

    // Testing USE the new generated hotstart file
    // Start Simulation 3
    swmm_open((char *)"hotstart/Simulation3.inp",
              (char *)"hotstart/Simulation3.rpt",
              (char *)"hotstart/Simulation3.out");
    swmm_start(0);
    // Iterate over nodes before stepping
    error = swmm_countObjects(SM_NODE, &number_of_nodes);
    for (index=0; index<number_of_nodes; index++)
    {
        // Confirm that neither values are zero
        read_val = Node[index].initDepth;
        BOOST_CHECK_SMALL(read_val - 0.00, 0.0000001);
        set_val = Node[index].oldDepth;
        BOOST_CHECK_SMALL(set_val - 0.00, 0.0000001);
        // Confirm diff
        BOOST_CHECK_SMALL(read_val - set_val, 0.0000001);
    }
    error = swmm_step(&elapsedTime);
    BOOST_CHECK_EQUAL(ERR_NONE, error);
    swmm_end();
    swmm_close();
}

BOOST_AUTO_TEST_SUITE_END()
