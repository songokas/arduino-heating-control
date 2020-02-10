#include "catch.hpp"

#include <Arduino.h>
#include <Time.h>
#include "Heating/Common.h"
#include "Heating/Config.h"
#include "Heating/Domain/HeaterInfo.h"

using Heating::Domain::StaticHeaterInfo;

SCENARIO( "heater state changes", "[heater]" ) {

    GIVEN( "given empty heater" ) {

        initialize_mock_arduino();
        setTime(100);
        StaticHeaterInfo<4> heater;
        
        REQUIRE( heater.isEmptyHistory() == true );
        
        WHEN( "heater state changes to on" ) {
            
            heater.markOn();
            
            THEN( "it should be on" ) {
                REQUIRE( heater.isOn() == true );
                
                REQUIRE( heater.getHistory(0).dtOn > 0 );
                REQUIRE( heater.getHistory(0).dtOff == 0 );
                REQUIRE( heater.getHistory(1).dtOn == 0 );
                REQUIRE( heater.getHistory(2).dtOn == 0 );

                WHEN( "heater state changes to off" ) {

                    heater.markOff();

                    THEN( "it should be off" ) {
                        REQUIRE( heater.isOn() == false );
                        REQUIRE( heater.getHistory(0).dtOn > 0 );
                        REQUIRE( heater.getHistory(0).dtOff > 0 );
                        REQUIRE( heater.getHistory(1).dtOn == 0 );
                        REQUIRE( heater.getHistory(2).dtOn == 0 );

                        WHEN( "heater state changes to on" ) {
                            
                            heater.markOn();
                            
                            THEN( "it should be on" ) {
                                REQUIRE( heater.isOn() == true );
                                REQUIRE( heater.getHistory(0).dtOn > 0 );
                                REQUIRE( heater.getHistory(0).dtOff == 0 );
                                REQUIRE( heater.getHistory(1).dtOn > 0 );
                                REQUIRE( heater.getHistory(1).dtOff > 0 );
                                REQUIRE( heater.getHistory(2).dtOn == 0 );

                                WHEN( "heater state changes to off" ) {

                                    heater.markOff();

                                    THEN( "it should be off" ) {
                                        REQUIRE( heater.isOn() == false );
                                        REQUIRE( heater.getHistory(0).dtOn > 0 );
                                        REQUIRE( heater.getHistory(0).dtOff > 0 );
                                        REQUIRE( heater.getHistory(1).dtOn > 0 );
                                        REQUIRE( heater.getHistory(1).dtOff > 0 );
                                        REQUIRE( heater.getHistory(2).dtOn == 0 );
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        WHEN( "heater is marked off" ) {

            heater.markOff();
            
            THEN( "heater state chages and time is recorded" ) {
                REQUIRE( heater.isOn() == false );
                REQUIRE( heater.getHistory(0).dtOn == 0 );
                REQUIRE( heater.getHistory(0).dtOff == 0 );
            }
        }
    }
}
