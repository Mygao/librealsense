/////////////////////////////////////////////////////////
// This set of tests is valid only for the R200 camera //
/////////////////////////////////////////////////////////

#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"

#include "unit-tests-common.h"

#include <climits>
#include <sstream>

TEST_CASE( "R200 metadata enumerates correctly", "[live] [r200]" )
{
    // Require at least one device to be plugged in
    safe_context ctx;
    const int device_count = rs_get_device_count(ctx, require_no_error());
    REQUIRE(device_count > 0);

    // For each device
    for(int i=0; i<device_count; ++i)
    {
        rs_device * dev = rs_get_device(ctx, 0, require_no_error());    
        REQUIRE(dev != nullptr);

        SECTION( "device name is Intel RealSense R200" )
        {
            const char * name = rs_get_device_name(dev, require_no_error());
            REQUIRE(name == std::string("Intel RealSense R200"));
        }

        SECTION( "device serial number has ten decimal digits" )
        {
            const char * serial = rs_get_device_serial(dev, require_no_error());
            REQUIRE(strlen(serial) == 10);
            for(int i=0; i<10; ++i) REQUIRE(isdigit(serial[i]));
        }

        SECTION( "device supports standard picture options and R200 extension options, and nothing else" )
        {
            for(int i=0; i<RS_OPTION_COUNT; ++i)
            {
                if(i >= RS_OPTION_COLOR_BACKLIGHT_COMPENSATION && i <= RS_OPTION_COLOR_WHITE_BALANCE)
                {
                    REQUIRE(rs_device_supports_option(dev, (rs_option)i, require_no_error()) == 1);
                }
                else if(i >= RS_OPTION_R200_LR_AUTO_EXPOSURE_ENABLED && i <= RS_OPTION_R200_DISPARITY_SHIFT)
                {
                    REQUIRE(rs_device_supports_option(dev, (rs_option)i, require_no_error()) == 1);
                }
                else
                {
                    REQUIRE(rs_device_supports_option(dev, (rs_option)i, require_no_error()) == 0);
                }
            }
        }
    }
}

///////////////////////////////////
// Calibration information tests //
///////////////////////////////////

TEST_CASE( "R200 device extrinsics are within expected parameters", "[live] [r200]" )
{
    // Require at least one device to be plugged in
    safe_context ctx;
    const int device_count = rs_get_device_count(ctx, require_no_error());
    REQUIRE(device_count > 0);

    // For each device
    for(int i=0; i<device_count; ++i)
    {
        rs_device * dev = rs_get_device(ctx, 0, require_no_error());    
        REQUIRE(dev != nullptr);

        SECTION( "no extrinsic transformation between DEPTH and INFRARED" )
        {
            rs_extrinsics extrin;
            rs_get_device_extrinsics(dev, RS_STREAM_DEPTH, RS_STREAM_INFRARED, &extrin, require_no_error());

            require_identity_matrix(extrin.rotation);
            require_zero_vector(extrin.translation);
        }

        SECTION( "only x-axis translation (~70 mm) between DEPTH and INFRARED2" )
        {
            rs_extrinsics extrin;
            rs_get_device_extrinsics(dev, RS_STREAM_DEPTH, RS_STREAM_INFRARED2, &extrin, require_no_error());

            require_identity_matrix(extrin.rotation);
            REQUIRE( extrin.translation[0] < -0.06f ); // Some variation is allowed, but should report at least 60 mm in all cases
            REQUIRE( extrin.translation[0] > -0.08f ); // Some variation is allowed, but should report at most 80 mm in all cases
            REQUIRE( extrin.translation[1] == 0.0f );
            REQUIRE( extrin.translation[2] == 0.0f );
        }

        SECTION( "only translation between DEPTH and RECTIFIED_COLOR" )
        {
            rs_extrinsics extrin;
            rs_get_device_extrinsics(dev, RS_STREAM_DEPTH, RS_STREAM_RECTIFIED_COLOR, &extrin, require_no_error());

            require_identity_matrix(extrin.rotation);
        }

        SECTION( "depth scale is 0.001 (by default)" )
        {
            REQUIRE( rs_get_device_depth_scale(dev, require_no_error()) == 0.001f );
        }
    }
}

TEST_CASE( "R200 infrared2 streaming modes exactly match infrared streaming modes", "[live] [r200]" )
{
    // Require at least one device to be plugged in
    safe_context ctx;
    const int device_count = rs_get_device_count(ctx, require_no_error());
    REQUIRE(device_count > 0);

    // For each device
    for(int i=0; i<device_count; ++i)
    {
        rs_device * dev = rs_get_device(ctx, 0, require_no_error());    
        REQUIRE(dev != nullptr);

        // Require that there are a nonzero amount of infrared modes, and that infrared2 has the same number of modes
        const int infrared_mode_count = rs_get_stream_mode_count(dev, RS_STREAM_INFRARED, require_no_error());
        REQUIRE( infrared_mode_count > 0 );
        REQUIRE( rs_get_stream_mode_count(dev, RS_STREAM_INFRARED2, require_no_error()) == infrared_mode_count );

        // For each streaming mode
        for(int j=0; j<infrared_mode_count; ++j)
        {
            // Require that INFRARED and INFRARED2 streaming modes are exactly identical
            int infrared_width = 0, infrared_height = 0, infrared_framerate = 0; rs_format infrared_format = RS_FORMAT_ANY;
            rs_get_stream_mode(dev, RS_STREAM_INFRARED, j, &infrared_width, &infrared_height, &infrared_format, &infrared_framerate, require_no_error());

            int infrared2_width = 0, infrared2_height = 0, infrared2_framerate = 0; rs_format infrared2_format = RS_FORMAT_ANY;
            rs_get_stream_mode(dev, RS_STREAM_INFRARED2, j, &infrared2_width, &infrared2_height, &infrared2_format, &infrared2_framerate, require_no_error());

            REQUIRE( infrared_width == infrared2_width );
            REQUIRE( infrared_height == infrared2_height );
            REQUIRE( infrared_format == infrared2_format );
            REQUIRE( infrared_framerate == infrared2_framerate );

            // Require that the intrinsics for these streaming modes match exactly
            rs_enable_stream(dev, RS_STREAM_INFRARED, infrared_width, infrared_height, infrared_format, infrared_framerate, require_no_error());
            rs_enable_stream(dev, RS_STREAM_INFRARED2, infrared2_width, infrared2_height, infrared2_format, infrared2_framerate, require_no_error());

            REQUIRE( rs_get_stream_format(dev, RS_STREAM_INFRARED, require_no_error()) == rs_get_stream_format(dev, RS_STREAM_INFRARED2, require_no_error()) );
            REQUIRE( rs_get_stream_framerate(dev, RS_STREAM_INFRARED, require_no_error()) == rs_get_stream_framerate(dev, RS_STREAM_INFRARED2, require_no_error()) );

            rs_intrinsics infrared_intrin = {}, infrared2_intrin = {};
            rs_get_stream_intrinsics(dev, RS_STREAM_INFRARED, &infrared_intrin, require_no_error());
            rs_get_stream_intrinsics(dev, RS_STREAM_INFRARED2, &infrared2_intrin, require_no_error());
            REQUIRE( infrared_intrin.width  == infrared_intrin.width  );
            REQUIRE( infrared_intrin.height == infrared_intrin.height );
            REQUIRE( infrared_intrin.ppx    == infrared_intrin.ppx    );
            REQUIRE( infrared_intrin.ppy    == infrared_intrin.ppy    );
            REQUIRE( infrared_intrin.fx     == infrared_intrin.fx     );
            REQUIRE( infrared_intrin.fy     == infrared_intrin.fy     );
            REQUIRE( infrared_intrin.model  == infrared_intrin.model  );
            for(int k=0; k<5; ++k) REQUIRE( infrared_intrin.coeffs[k]  == infrared_intrin.coeffs[k] );
        }
    }
}

/////////////////////
// Streaming tests //
/////////////////////

TEST_CASE( "a single R200 can stream a variety of reasonable streaming mode combinations", "[live] [r200] [one-camera]" )
{
    safe_context ctx;
    
    SECTION( "exactly one device is connected" )
    {
        int device_count = rs_get_device_count(ctx, require_no_error());
        REQUIRE(device_count == 1);
    }

    rs_device * dev = rs_get_device(ctx, 0, require_no_error());
    REQUIRE(dev != nullptr);

    SECTION( "device name is Intel RealSense R200" )
    {
        const char * name = rs_get_device_name(dev, require_no_error());
        REQUIRE(name == std::string("Intel RealSense R200"));
    }

    SECTION( "streaming is possible in some reasonable configurations" )
    {
        test_streaming(dev, {
            {RS_STREAM_DEPTH, 480, 360, RS_FORMAT_Z16, 60}
        });

        test_streaming(dev, {
            {RS_STREAM_DEPTH, 480, 360, RS_FORMAT_Z16, 60},
            {RS_STREAM_COLOR, 640, 480, RS_FORMAT_RGB8, 60}
        });

        test_streaming(dev, {
            {RS_STREAM_DEPTH, 480, 360, RS_FORMAT_Z16, 60},
            {RS_STREAM_INFRARED, 480, 360, RS_FORMAT_Y8, 60}
        });

        test_streaming(dev, {
            {RS_STREAM_INFRARED, 492, 372, RS_FORMAT_Y16, 60},
            {RS_STREAM_INFRARED2, 492, 372, RS_FORMAT_Y16, 60}
        });

        test_streaming(dev, {
            {RS_STREAM_DEPTH, 480, 360, RS_FORMAT_Z16, 60},
            {RS_STREAM_COLOR, 640, 480, RS_FORMAT_RGB8, 60},
            {RS_STREAM_INFRARED, 480, 360, RS_FORMAT_Y8, 60},
            {RS_STREAM_INFRARED2, 480, 360, RS_FORMAT_Y8, 60}
        });
    }
}

/////////////
// Options //
/////////////

TEST_CASE( "R200 options can be queried and set", "[live] [r200]" )
{
    // Require at least one device to be plugged in
    safe_context ctx;
    const int device_count = rs_get_device_count(ctx, require_no_error());
    REQUIRE(device_count > 0);

    // For each device
    for(int i=0; i<device_count; ++i)
    {
        rs_device * dev = rs_get_device(ctx, 0, require_no_error());    
        REQUIRE(dev != nullptr);

        rs_enable_stream_preset(dev, RS_STREAM_DEPTH, RS_PRESET_BEST_QUALITY, require_no_error());
        rs_start_device(dev, require_no_error());        

        std::this_thread::sleep_for(std::chrono::seconds(1));

        test_option(dev, RS_OPTION_R200_LR_AUTO_EXPOSURE_ENABLED, {0, 1}, {});
        test_option(dev, RS_OPTION_R200_LR_GAIN, {100, 200, 400, 800, 1600}, {}); // Gain percentage
        test_option(dev, RS_OPTION_R200_LR_EXPOSURE, {40, 80, 160}, {}); // Tenths of milliseconds
        test_option(dev, RS_OPTION_R200_EMITTER_ENABLED, {0, 1}, {});
        test_option(dev, RS_OPTION_R200_DEPTH_CONTROL_PRESET, {0, 1, 2, 3, 4, 5}, {});
        test_option(dev, RS_OPTION_R200_DEPTH_UNITS, {0, 1, 2, 3, 4, 5}, {});
        test_option(dev, RS_OPTION_R200_DEPTH_CLAMP_MIN, {0, 500, 1000, 2000}, {});
        test_option(dev, RS_OPTION_R200_DEPTH_CLAMP_MAX, {500, 1000, 2000, USHRT_MAX}, {});
        test_option(dev, RS_OPTION_R200_DISPARITY_MODE_ENABLED, {0, 1}, {});        

        rs_stop_device(dev, require_no_error());
        rs_disable_stream(dev, RS_STREAM_DEPTH, require_no_error());
    }
}