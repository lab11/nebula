# Dockerfile

# Use the official Python image as the base image
FROM python:3.9

# Set the working directory
WORKDIR /app

# Copy the requirements.txt file into the container
COPY requirements.txt .

# Install the required packages
RUN pip install --no-cache-dir -r requirements.txt

# Copy the rest of the application code into the container
COPY . .

# Expose the port the app runs on
EXPOSE 8000

# Set the environment variable for the server mode (provider or app)
ENV SERVER_MODE=provider

# Start the Uvicorn server with the specified server mode
CMD if [ "$SERVER_MODE" = "provider" ] ; then \
        uvicorn main:app --host 0.0.0.0 --port 8000 --workers 4; \
    else \
        uvicorn main:app --host 0.0.0.0 --port 8000 --workers 4; \
    fi

