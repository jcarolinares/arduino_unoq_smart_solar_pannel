import time

from arduino.app_utils import App
from arduino.app_bricks.weather_forecast import WeatherForecast

forecast_result = ""

def loop():
    """This function is called repeatedly by the App framework."""
    
    forecaster = WeatherForecast()
    city = "Turin"
    forecast = forecaster.get_forecast_by_city(city)
    print(f"The weather forecast for {city} says it will be {forecast.category} ({forecast.description}).")

    forecast_result = forecast.category
    # forecast_result = "snowy"
    
    if forecast.category == "snowy" or forecast.category == "foggy":
        print("Conditions are critically bad, protecting solar pannels")
        
    
    # You can replace this with any code you want your App to run repeatedly.
    time.sleep(10)


# See: https://docs.arduino.cc/software/app-lab/tutorials/getting-started/#app-run
App.run(user_loop=loop)


