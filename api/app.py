
import os
import json
import secrets
from datetime import datetime, timezone
from typing import Literal

import paho.mqtt.publish as publish
from dotenv import load_dotenv
from fastapi import FastAPI, Header, HTTPException, Query, Response
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
from pymongo import MongoClient
from pymongo.errors import DuplicateKeyError

load_dotenv()

MONGODB_URI = os.getenv("MONGODB_URI")
MQTT_NAME = os.getenv("MQTT_NAME")

client = MongoClient(MONGODB_URI)
db = client["ecse3038_test2"]

fans = db["fans"]
readings = db["readings"]

fans.create_index("mac", unique=True)

app = FastAPI()


# Request models

class Fan(BaseModel):
    mac: str
    room: str


class Reading(BaseModel):
    mac: str
    humidity: float = Field(ge=0, le=100)


class FanSettings(BaseModel):
    mode: Literal["auto", "on", "off"]
    max_humidity: float = Field(ge=0, le=100)


# Helper function

def check_api_key(mac, api_key):
    if not api_key:
        raise HTTPException(status_code=401, detail="API key required")

    fan = fans.find_one({"api_key": api_key})

    if fan is None:
        raise HTTPException(status_code=401, detail="Invalid API key")

    if fan["mac"] != mac:
        raise HTTPException(status_code=403, detail="Wrong fan API key")

    return fan


def clean_fan(fan):
    return {
        "mac": fan["mac"],
        "room": fan["room"],
        "settings": fan.get("settings")
    }


# Home route

@app.get("/")
def home():
    return {"message": "Bathroom Fan API is running"}


# A1: Register a fan

@app.post("/fans", status_code=201)
def register_fan(data: Fan):

    api_key = secrets.token_urlsafe(16)

    fan = {
        "mac": data.mac,
        "room": data.room,
        "api_key": api_key,
        "settings": None
    }

    try:
        fans.insert_one(fan)
    except DuplicateKeyError:
        raise HTTPException(status_code=409, detail="Fan already exists")

    return {
        "mac": data.mac,
        "room": data.room,
        "api_key": api_key
    }


# A1: Get all fans

@app.get("/fans")
def get_fans():

    results = fans.find()

    return [clean_fan(fan) for fan in results]


# A1: Get one fan

@app.get("/fans/{mac}")
def get_fan(mac: str):

    fan = fans.find_one({"mac": mac})

    if fan is None:
        raise HTTPException(status_code=404, detail="Fan not found")

    return clean_fan(fan)


# A2: Submit humidity reading

@app.post("/readings", status_code=201)
def create_reading(
    data: Reading,
    x_api_key: str = Header(default=None, alias="X-API-Key")
):
    check_api_key(data.mac, x_api_key)

    new_reading = {
        "mac": data.mac,
        "humidity": data.humidity,
        "timestamp": datetime.now(timezone.utc)
    }

    readings.insert_one(new_reading)

    return {
        "mac": new_reading["mac"],
        "humidity": new_reading["humidity"],
        "timestamp": new_reading["timestamp"]
    }


# A3: Get readings

@app.get("/readings")
def get_readings(
    mac: str = None,
    limit: int = Query(default=10, ge=1, le=100)
):

    search = {}

    if mac:
        search["mac"] = mac

    results = readings.find(search).sort("timestamp", -1).limit(limit)

    return [
        {
            "mac": item["mac"],
            "humidity": item["humidity"],
            "timestamp": item["timestamp"]
        }
        for item in results
    ]


# A4: Update fan settings

@app.put("/fans/{mac}/settings")
def update_settings(mac: str, data: FanSettings):

    fan = fans.find_one({"mac": mac})

    if fan is None:
        raise HTTPException(status_code=404, detail="Fan not found")

    is_new = fan.get("settings") is None

    settings = {
        "mode": data.mode,
        "max_humidity": data.max_humidity
    }

    fans.update_one(
        {"mac": mac},
        {"$set": {"settings": settings}}
    )

    topic = f"ecse3038/{MQTT_NAME}/fan/settings"

    try:
        publish.single(
            topic,
            payload=json.dumps(settings),
            hostname="135.232.203.144",
            port=1883,
            retain=True
        )
    except Exception as error:
        raise HTTPException(status_code=502, detail=str(error))

    return JSONResponse(
        status_code=201 if is_new else 200,
        content=settings
    )


# A5: Delete a fan

@app.delete("/fans/{mac}", status_code=204)
def delete_fan(
    mac: str,
    x_api_key: str = Header(default=None, alias="X-API-Key")
):

    check_api_key(mac, x_api_key)

    fans.delete_one({"mac": mac})
    readings.delete_many({"mac": mac})

    return Response(status_code=204)
