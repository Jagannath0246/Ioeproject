from flask import Flask, render_template, request, jsonify
from flask_pymongo import PyMongo
from datetime import datetime
import os

app = Flask(__name__)

# MongoDB configuration
app.config["MONGO_URI"] = "mongodb+srv://JMS:Jms0246810@taskmanager.cenfmu6.mongodb.net/water_tank?retryWrites=true&w=majority&appName=WaterTankApp"
mongo = PyMongo(app)

# Collections
sensor_data_collection = mongo.db.sensor_data
commands_collection = mongo.db.commands

@app.route('/')
def index():
    """Main dashboard page"""
    return render_template('index.html')

@app.route('/api/sensor-data', methods=['POST'])
def receive_sensor_data():
    """Receive sensor data from ESP32"""
    try:
        data = request.get_json()

        # Add timestamp
        data['timestamp'] = datetime.utcnow()

        # Insert into MongoDB
        result = sensor_data_collection.insert_one(data)

        print(f"Received sensor data: {data}")

        return jsonify({
            'success': True,
            'message': 'Data received successfully',
            'id': str(result.inserted_id)
        }), 200

    except Exception as e:
        print(f"Error receiving sensor data: {str(e)}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@app.route('/api/commands', methods=['GET'])
def get_commands():
    """Get pending commands for ESP32"""
    try:
        # Get the latest command
        command = commands_collection.find_one(
            {'executed': False},
            sort=[('timestamp', -1)]
        )

        if command:
            # Mark command as executed
            commands_collection.update_one(
                {'_id': command['_id']},
                {'$set': {'executed': True}}
            )

            return jsonify({
                'motor_command': command.get('motor_command', ''),
                'timestamp': command.get('timestamp')
            }), 200
        else:
            return jsonify({}), 200

    except Exception as e:
        print(f"Error getting commands: {str(e)}")
        return jsonify({'error': str(e)}), 500

@app.route('/api/motor-control', methods=['POST'])
def motor_control():
    """Manual motor control from web interface"""
    try:
        data = request.get_json()
        command = data.get('command')  # 'start', 'stop', or 'auto'

        # Insert command into database
        command_doc = {
            'motor_command': command,
            'timestamp': datetime.utcnow(),
            'executed': False,
            'source': 'web_interface'
        }

        commands_collection.insert_one(command_doc)

        return jsonify({
            'success': True,
            'message': f'Motor command "{command}" sent successfully'
        }), 200

    except Exception as e:
        print(f"Error in motor control: {str(e)}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@app.route('/api/latest-data', methods=['GET'])
def get_latest_data():
    """Get latest sensor readings for web dashboard"""
    try:
        # Get the latest sensor data
        latest_data = sensor_data_collection.find_one(
            {},
            sort=[('timestamp', -1)]
        )

        if latest_data:
            # Convert ObjectId to string for JSON serialization
            latest_data['_id'] = str(latest_data['_id'])
            latest_data['timestamp'] = latest_data['timestamp'].isoformat()

            return jsonify(latest_data), 200
        else:
            return jsonify({'message': 'No data available'}), 404

    except Exception as e:
        print(f"Error getting latest data: {str(e)}")
        return jsonify({'error': str(e)}), 500

@app.route('/api/historical-data', methods=['GET'])
def get_historical_data():
    """Get historical sensor data for charts"""
    try:
        # Get last 50 readings
        historical_data = list(sensor_data_collection.find(
            {},
            sort=[('timestamp', -1)]
        ).limit(50))

        # Convert ObjectId and datetime for JSON
        for data in historical_data:
            data['_id'] = str(data['_id'])
            data['timestamp'] = data['timestamp'].isoformat()

        # Reverse to get chronological order
        historical_data.reverse()

        return jsonify(historical_data), 200

    except Exception as e:
        print(f"Error getting historical data: {str(e)}")
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)
