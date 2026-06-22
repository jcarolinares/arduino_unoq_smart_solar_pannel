from arduino.app_utils import *
from arduino.app_bricks.weather_forecast import WeatherForecast
from datetime import datetime, UTC
import time

# Object Flags Variables
weather_alert = False
forecast_result_debug = ""

def loop():
    """This function is called repeatedly by the App framework."""

    global weather_alert
    
    forecaster = WeatherForecast()
    city = "Turin"
    forecast = forecaster.get_forecast_by_city(city)
    print(f"The weather forecast for {city} says it will be {forecast.category} ({forecast.description}).")

    
    if (forecast.category == "snowy" or forecast.category == "foggy" or forecast_result_debug == "snowy") and weather_alert == False :
        print("Conditions are critically bad, protecting solar pannels")
        weather_alert = True
        Bridge.call("set_weather_alert", weather_alert)
    elif weather_alert == True and (forecast.category != "snowy" and forecast.category != "foggy" and forecast_result_debug != "snowy") :
        print("Conditions improved after critical alert, recovering normal functioning.")
        weather_alert = False
        Bridge.call("set_weather_alert", weather_alert)        
        
         
    # You can replace this with any code you want your App to run repeatedly.
    time.sleep(60)


# See: https://docs.arduino.cc/software/app-lab/tutorials/getting-started/#app-run
App.run(user_loop=loop)