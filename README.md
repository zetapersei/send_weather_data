# Send Weather Data

`send_weather_data` is a small C program that reads the latest weather-station
measurements from a MySQL database and prepares them for submission to the
Weather Underground weather-station update endpoint.

The database is expected to contain data collected by
[WeatherReader](https://github.com/hassebjork/WeatherReader). The program runs
once per invocation; it does not poll the database or run as a background
service.

## What it reads

The program connects to a MySQL server on `localhost` and retrieves:

- The latest temperature and humidity readings for sensor ID `1`.
- The latest barometer reading.
- Today's first and latest rain totals, used to calculate daily rainfall.
- A rain total from the last 60 minutes, used to calculate hourly rainfall.
- The latest wind speed, gust, and direction.

It prints the collected values to the console. The upload routine is intended
to convert temperature to Fahrenheit, rainfall to inches, wind speed and gust
to miles per hour, and pressure to inches of mercury before submitting the
observations.

## Requirements

- A C compiler supporting C99 (such as GCC).
- `make`.
- MySQL or MariaDB client development libraries and the `mysql_config` utility.
- libcurl development files.
- A MySQL database populated with the tables and columns expected by
  WeatherReader (`wr_temperature`, `wr_humidity`, `wr_barometer`, `wr_rain`,
  and `wr_wind`).

## Build and run

From the project directory:

```sh
make
./send_data
```

To remove the generated executable:

```sh
make clean
```

## Configuration and current limitations

Database connection settings are currently hard-coded in `send_data.c`. The
connection uses `localhost`, and the username, password, and database name
are placeholders (`user`, `passwd`, and `database`). Replace them with values
for your installation before running the program.

The Weather Underground station settings and POST payload are also
placeholders in the current source. In particular, the request body does not
currently include the measurements fetched from MySQL, so a successful run
does not mean that observations have been uploaded. Configure the station
credentials and complete the request payload before relying on this program
to publish data.
