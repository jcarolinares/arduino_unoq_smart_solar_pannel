#  UNO Q Smart Solar Pannel System
#  Created by Julián Caro Linares for Arduino INC
#  CC-BY-SA


from arduino.app_utils import *
from arduino.app_bricks.weather_forecast import WeatherForecast
from arduino.app_bricks.dbstorage_tsstore import TimeSeriesStore
from arduino.app_bricks.web_ui import WebUI
from datetime import datetime, UTC
import time

# Object Flags Variables
weather_alert = False
forecast_result_debug = ""

# Timer for Weather Polling
last_weather_check = 0

# Cache variables for the Web UI
latest_weather_cat = "--"
latest_weather_desc = "Fetching forecast..."

# Initialize Telemetry Bricks
db = TimeSeriesStore()
ui = WebUI()

# Expose API so the HTML frontend can fetch historical graph data
def on_get_samples(resource: str, start: str, aggr_window: str):
    samples = db.read_samples(measure=resource, start_from=start, aggr_window=aggr_window, aggr_func="mean", limit=100)
    res = []
    for sample in samples:
        point = {
            "ts": sample[1],
            "value": sample[2],
        }
        res.append(point)
    return res

ui.expose_api("GET", "/get_samples/{resource}/{start}/{aggr_window}", on_get_samples)

def loop():
    """This function is called repeatedly by the App framework."""
    global weather_alert, last_weather_check, latest_weather_cat, latest_weather_desc
    
    current_time = time.time()

    # ---------------------------------------------------------
    # 1. WEATHER FORECAST & ALERT SYSTEM (Runs every 60 seconds)
    # ---------------------------------------------------------
    if current_time - last_weather_check >= 60:
        forecaster = WeatherForecast()
        city = "Turin"
        forecast = forecaster.get_forecast_by_city(city)
        print(f"The weather forecast for {city} says it will be {forecast.category} ({forecast.description}).")

        # Save the results to global variables so the UI can read them
        latest_weather_cat = forecast.category
        latest_weather_desc = forecast.description

        if (forecast.category == "snowy" or forecast.category == "foggy" or forecast_result_debug == "snowy") and weather_alert == False:
            print("Conditions are critically bad, protecting solar pannels")
            weather_alert = True
            Bridge.call("set_weather_alert", weather_alert)
        elif weather_alert == True and (forecast.category != "snowy" and forecast.category != "foggy" and forecast_result_debug != "snowy"):
            print("Conditions improved after critical alert, recovering normal functioning.")
            weather_alert = False
            Bridge.call("set_weather_alert", weather_alert)
            
        # Update the timer
        last_weather_check = current_time
        
    # ---------------------------------------------------------
    # 2. TELEMETRY & WEB UI SYSTEM (Runs every loop iteration)
    # ---------------------------------------------------------
    ts = int(datetime.now().timestamp() * 1000)
    
    # Push the latest cached weather to the UI
    if latest_weather_cat != "--":
        ui.send_message('weather_update', {
            "category": latest_weather_cat,
            "description": latest_weather_desc
        })

    try:
        # Ask the Microcontroller for the sensor values
        solar_val = Bridge.call("get_solar")
        ir_val = Bridge.call("get_infrared")
        
        # Ask the Microcontroller for the live servo positions
        pan_val = Bridge.call("get_bottom")
        tilt_val = Bridge.call("get_top")
        
        # Ask the Microcontroller for Local Environment Data
        temp_val = Bridge.call("get_temp")
        hum_val = Bridge.call("get_humidity")
        
        # Store and send Solar Panel Voltage
        if solar_val is not None:
            db.write_sample('solar', solar_val, ts)
            ui.send_message('solar_update', {"value": solar_val, "ts": ts})

        # Store and send Modulino Infrared
        if ir_val is not None:
            db.write_sample('infrared', ir_val, ts)
            ui.send_message('ir_update', {"value": ir_val, "ts": ts})
            
        # Send Live Servo Positions
        if pan_val is not None and tilt_val is not None:
            ui.send_message('position_update', {"pan": pan_val, "tilt": tilt_val})

        # Send Live Local Environment Data (Rounding to 1 decimal for a clean UI)
        if temp_val is not None and hum_val is not None:
            ui.send_message('env_update', {"temp": round(float(temp_val), 1), "humidity": round(float(hum_val), 1)})
            
    except Exception as e:
        print(f"Waiting for Arduino Bridge to be ready... ({e})")

    # We sleep for 1 second to keep the Web UI responsive!
    time.sleep(0.2)

# See: https://docs.arduino.cc/software/app-lab/tutorials/getting-started/#app-run
App.run(user_loop=loop)