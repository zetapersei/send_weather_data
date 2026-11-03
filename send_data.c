/*
*************************************************************************
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
   IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF OR INABILITY TO USE THIS SOFTWARE, EVEN IF THE COPYRIGHT HOLDERS OR
   CONTRIBUTORS ARE AWARE OF THE POSSIBILITY OF SUCH DAMAGE.
************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <curl/curl.h>
#include <mysql.h>

static const char URL_DATA[] = "http://weatherstation.wunderground.com/weatherstation/updateweatherstation.php";
static const char ID_DATA[] = "station_id";
static const char PASSWD_DATA[] = "user_pwd";

static int fetch_single_value(MYSQL *connection, const char *query,
                              char *buffer, size_t buffer_size)
{
    MYSQL_RES *result = NULL;
    MYSQL_ROW row;
    int status;

    status = mysql_query(connection, query);
    if (status != 0) {
        fprintf(stderr, "%s\n", mysql_error(connection));
        return status;
    }

    result = mysql_store_result(connection);
    if (result == NULL) {
        fprintf(stderr, "%s\n", mysql_error(connection));
        return 1;
    }

    row = mysql_fetch_row(result);
    if (row == NULL || row[0] == NULL) {
        if (buffer != NULL && buffer_size > 0) {
            buffer[0] = '\0';
        }
        mysql_free_result(result);
        return 0;
    }

    snprintf(buffer, buffer_size, "%s", row[0]);
    mysql_free_result(result);
    return 0;
}

static int fetch_wind_values(MYSQL *connection,
                             char *wind_speed, size_t wind_speed_size,
                             char *wind_gust, size_t wind_gust_size,
                             char *wind_dir, size_t wind_dir_size)
{
    MYSQL_RES *result = NULL;
    MYSQL_ROW row;
    int status;

    status = mysql_query(connection,
                         "SELECT speed, gust, dir FROM wr_wind ORDER BY time DESC LIMIT 1");
    if (status != 0) {
        fprintf(stderr, "%s\n", mysql_error(connection));
        return status;
    }

    result = mysql_store_result(connection);
    if (result == NULL) {
        fprintf(stderr, "%s\n", mysql_error(connection));
        return 1;
    }

    row = mysql_fetch_row(result);
    if (row != NULL) {
        snprintf(wind_speed, wind_speed_size, "%s", row[0] != NULL ? row[0] : "0");
        snprintf(wind_gust, wind_gust_size, "%s", row[1] != NULL ? row[1] : "0");
        snprintf(wind_dir, wind_dir_size, "%s", row[2] != NULL ? row[2] : "0");
    }

    mysql_free_result(result);
    return 0;
}

void wa_send_data(float ctemp, float humid, float day_amount, float hour_amount,
                  float windspeed, float windg, float winddir, float barohpa)
{
    CURL *curl;
    CURLcode res;
    char postdata[512];
    const float gamma = logf(humid / 100.0f) + (17.27f * ctemp / (237.7f + ctemp));
    const float dp = 237.7f * gamma / (17.27f - gamma);
    const float ftemp = (ctemp * 9.0f / 5.0f) + 32.0f;
    const float dewptf = (dp * 9.0f / 5.0f) + 32.0f;
    const float incday_amount = day_amount / 25.445f;
    const float inchour_amount = hour_amount / 25.445f;
    const float windgmph = windg * 2.236f;
    const float windspeedmph = windspeed * 2.236f;
    const float barominch = (barohpa + 11.0f) / 33.865f;
    const int written = snprintf(postdata, sizeof(postdata),
           "action=updateraw&ID=%s&PASSWORD=%s&dateutc=now&tempf=%.1f&humidity=%.0f&dewptf=%.1f&rainin=%.2f&hourlyrainin=%.2f&windspeedmph=%.1f&windgustmph=%.1f&winddir=%.0f&baromin=%.3f",
           ID_DATA,
           PASSWD_DATA,
           ftemp,
           humid,
           dewptf,
           incday_amount,
           inchour_amount,
           windspeedmph,
           windgmph,
           winddir,
           barominch);

    if (written < 0 || (size_t)written >= sizeof(postdata)) {
        fprintf(stderr, "Post payload too large for buffer.\n");
        return;
    }

    curl = curl_easy_init();
    if (!curl) {
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, URL_DATA);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)written);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);
}

/*
   Retrieve data from Mysql database and send data with http post.
   This application uses data stored by https://github.com/hassebjork/WeatherReader.git
   and sends sensor values to the Weather Underground site.
*/
int main(void)
{
    MYSQL mysql = {0};
    MYSQL *connection;
    char temperature[32] = "0";
    char humidity[32] = "0";
    char firstrain[32] = "0";
    char lastrain[32] = "0";
    char hourlastrain[32] = "0";
    char pressure[32] = "0";
    char wind_speed[32] = "0";
    char wind_gust[32] = "0";
    char wind_dir[32] = "0";
    float ftemperature;
    float fhumidity;
    float ffirstrain;
    float flastrain;
    float fhourlastrain;
    float fwind_speed;
    float fwind_gust;
    float fwind_dir;
    float diffrain;
    float diffhourrain;
    float fpressure;
    int status;

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "curl_global_init() failed\n");
        return 1;
    }

    connection = mysql_init(&mysql);
    if (connection == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        curl_global_cleanup();
        return 1;
    }

    if (mysql_real_connect(connection, "localhost", "user", "passwd", "database",
                           0, NULL, 0) == NULL) {
        fprintf(stderr, "%s\n", mysql_error(connection));
        mysql_close(connection);
        curl_global_cleanup();
        return 1;
    }

    status = fetch_single_value(connection,
                                "SELECT value FROM wr_temperature WHERE sensor_id = 1 ORDER BY time DESC LIMIT 1",
                                temperature, sizeof(temperature));
    if (status != 0) {
        mysql_close(connection);
        curl_global_cleanup();
        return 1;
    }

    status = fetch_single_value(connection,
                                "SELECT value FROM wr_humidity WHERE sensor_id = 1 ORDER BY time DESC LIMIT 1",
                                humidity, sizeof(humidity));
    if (status != 0) {
        mysql_close(connection);
        curl_global_cleanup();
        return 1;
    }

    status = fetch_single_value(connection,
                                "SELECT value FROM wr_barometer ORDER BY time DESC LIMIT 1",
                                pressure, sizeof(pressure));
    if (status != 0) {
        mysql_close(connection);
        curl_global_cleanup();
        return 1;
    }

    status = fetch_single_value(connection,
                                "SELECT total FROM wr_rain WHERE DATE(time) = DATE(CURDATE()) ORDER BY time DESC LIMIT 1",
                                lastrain, sizeof(lastrain));
    if (status != 0) {
        mysql_close(connection);
        curl_global_cleanup();
        return 1;
    }

    status = fetch_single_value(connection,
                                "SELECT total FROM wr_rain WHERE DATE(time) = DATE(CURDATE()) ORDER BY time LIMIT 1",
                                firstrain, sizeof(firstrain));
    if (status != 0) {
        mysql_close(connection);
        curl_global_cleanup();
        return 1;
    }

    status = fetch_single_value(connection,
                                "SELECT total FROM wr_rain WHERE TIMESTAMP(time) >= DATE_SUB(NOW(), INTERVAL 60 MINUTE) LIMIT 1",
                                hourlastrain, sizeof(hourlastrain));
    if (status != 0) {
        mysql_close(connection);
        curl_global_cleanup();
        return 1;
    }

    status = fetch_wind_values(connection, wind_speed, sizeof(wind_speed),
                              wind_gust, sizeof(wind_gust),
                              wind_dir, sizeof(wind_dir));
    if (status != 0) {
        mysql_close(connection);
        curl_global_cleanup();
        return 1;
    }

    ftemperature = strtof(temperature, NULL);
    fhumidity = strtof(humidity, NULL);
    ffirstrain = strtof(firstrain, NULL);
    flastrain = strtof(lastrain, NULL);
    fhourlastrain = strtof(hourlastrain, NULL);
    fwind_speed = strtof(wind_speed, NULL);
    fwind_gust = strtof(wind_gust, NULL);
    fwind_dir = strtof(wind_dir, NULL);
    fpressure = strtof(pressure, NULL);

    diffrain = flastrain - ffirstrain;
    diffhourrain = flastrain - fhourlastrain;

    printf("\nTEMP:%.1f\nHUMIDITY:%.0f\nRAIN:%.2f\nRAIN HOUR:%.2f\nWind Speed:%.1f\nWind Gust:%.1f\nWind Direction:%.0f\nPressure: %.0f\n",
           ftemperature, fhumidity, diffrain, diffhourrain,
           fwind_speed, fwind_gust, fwind_dir, fpressure);

    wa_send_data(ftemperature, fhumidity, diffrain, diffhourrain,
                 fwind_speed, fwind_gust, fwind_dir, fpressure);

    mysql_close(connection);
    curl_global_cleanup();
    return 0;
}
