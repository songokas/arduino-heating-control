#include "catch.hpp"


#include <Arduino.h>
#include <Time.h>
#include "Heating/Common.h"
#include "Heating/Config.h"
#include "Heating/Domain/ZoneInfo.h"

using Heating::Domain::ZoneInfo;
using Heating::Config;
using Heating::Error;

SCENARIO( "recording zone states", "[zone]" ) {

    GIVEN( "new zone info" ) {

        initialize_mock_arduino();
        setTime(100);
        ZoneInfo zone;

        REQUIRE( zone.stateTimes[0].dtOn == 0 );

        WHEN( "zone records valid state" ) {

            zone.recordState(100);
            
            THEN( "on should be recorded" ) {
                REQUIRE( zone.stateTimes[0].dtOn > 0 );
                REQUIRE( zone.stateTimes[0].dtOff == 0 );
                REQUIRE( zone.stateTimes[1].dtOn == 0 );
                REQUIRE( zone.stateTimes[1].dtOff == 0 );

                WHEN( "zone records invalid state" ) {

                    zone.recordState(0);

                    THEN( "off should be recorded" ) {
                        REQUIRE( zone.stateTimes[0].dtOn > 0 );
                        REQUIRE( zone.stateTimes[0].dtOff > 0 );
                        REQUIRE( zone.stateTimes[1].dtOn == 0 );
                        REQUIRE( zone.stateTimes[1].dtOff == 0 );

                        WHEN( "zone again records valid state" ) {

                            zone.recordState(100);
                            
                            THEN( "new on should be recorded" ) {
                                REQUIRE( zone.stateTimes[0].dtOn > 0 );
                                REQUIRE( zone.stateTimes[0].dtOff == 0 );
                                REQUIRE( zone.stateTimes[1].dtOn > 0 );
                                REQUIRE( zone.stateTimes[1].dtOff > 0 );

                                WHEN( "zone again records invalid state" ) {

                                    zone.recordState(0);

                                    THEN( "new off should be recorded" ) {
                                        REQUIRE( zone.stateTimes[0].dtOn > 0 );
                                        REQUIRE( zone.stateTimes[0].dtOff > 0 );
                                        REQUIRE( zone.stateTimes[1].dtOn > 0 );
                                        REQUIRE( zone.stateTimes[1].dtOff > 0 );
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        WHEN( "recording the same on state no times are created" ) {

            zone.recordState(100);
            zone.recordState(100);

            THEN( "same time is created" ) {
                REQUIRE( zone.stateTimes[0].dtOn > 0 );
                REQUIRE( zone.stateTimes[0].dtOff == 0 );
                REQUIRE( zone.stateTimes[1].dtOn == 0 );
                REQUIRE( zone.stateTimes[1].dtOff == 0 );
            }
        }

        WHEN( "recording the same off state no times are created" ) {

            zone.recordState(0);
            zone.recordState(0);

            THEN( "same time is created" ) {
                REQUIRE( zone.stateTimes[0].dtOn == 0 );
                REQUIRE( zone.stateTimes[0].dtOff > 0 );
                REQUIRE( zone.stateTimes[1].dtOn == 0 );
                REQUIRE( zone.stateTimes[1].dtOff == 0 );
            }
        }

       WHEN( "state and time will affect warm up times" ) {
            REQUIRE( zone.isWarm(0) == false);
            REQUIRE( zone.isOn() == false);
            zone.recordState(100);
            REQUIRE( zone.isWarm(0) == false);
            REQUIRE( zone.isOn() == false);
            zone.pin.state = 100;

            THEN( "should be warm after 1 second" ) {

                REQUIRE( zone.isOn() == true);
                REQUIRE( zone.isWarm(0) == true);
                REQUIRE( zone.isWarm(1) == false);
                delay(1000);
                REQUIRE( zone.isWarm(1) == true);
            }
        }

       WHEN( "recording an off state will not affect warm up times" ) {
            zone.pin.state = 20;
            zone.recordState(20);
            REQUIRE( zone.isOn() == true);
            REQUIRE( zone.isWarm(0) == true);
            zone.recordState(0);

            THEN( "should not be warm" ) {
                REQUIRE( zone.isOn() == true);
                REQUIRE( zone.isWarm(0) == false);
                zone.pin.state = 0;
                REQUIRE( zone.isOn() == false);
            }
        }
    }
}
