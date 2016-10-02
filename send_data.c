/* 
*************************************************************************
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
   IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT 
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
#include <curl/curl.h>
#include <mysql.h>

        static const char URL_DATA[] = "http://weatherstation.wunderground.com/weatherstation/updateweatherstation.php";
        static const char ID_DATA[] = "station_id";
        static const char PASSWD_DATA[] = "user_pwd";

        /* Function implementation */

        void wa_send_data ( float CTemp, float humid, float day_amount, float hour_amount, float windspeed, float windg, float winddir,float barohpa) 
        {

                CURL *curl;
                CURLcode res;

                float FTemp = CTemp*9/5 + 32;

                /* Dew Point simple formula: 
                        a = 17.27
                        b = 237.7
                        gamma = (a * t / (b + t)) + ln(rh / 100.0)
                        dew = b * gamma / (a - gamma)*/
		float gamma = logf(humid / 100) + (17.27 * CTemp / (237.7 + CTemp));
                float dp = 237.7 * gamma / (17.27 - gamma);
                float dewptf = dp * 9/5 + 32;
                float incday_amount = day_amount / 25.445;
                float inchour_amount = hour_amount / 25.445;
                float windgmph = (windg * 2.236);
		float windspeedmph = (windspeed * 2.236);
                float barominch = (barohpa + 11) / 33.865;
                
                char *postdata;
                        postdata = malloc(400 * sizeof(char));

                sprintf(postdata,"ID=%s&PASSWORD=%s&dateutc=now&tempf=%.1f&humidity=%.1f&dewptf=%.1f&dailyrainin=%.3f&rainin=%.3f&windspeedmph=%.2f&windgustmph=%.2f&winddir=%.0f&baromin=%.2f&action=updateraw",
                        ID_DATA, PASSWD_DATA, FTemp, humid, dewptf, incday_amount, inchour_amount, windspeedmph, windgmph, winddir,barominch);

                curl = curl_easy_init();

                if(curl) {
                        curl_easy_setopt(curl, CURLOPT_URL, URL_DATA);
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(postdata));
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);

res = curl_easy_perform(curl);

                        if(res != CURLE_OK)
                                fprintf(stderr, "curl_easy_perform() failed: %s\n",
                                curl_easy_strerror(res));

                        curl_easy_cleanup(curl);
                }

        }


/* Retrieve data from Mysql database and send data  with http post
   This application use data stored by https://github.com/hassebjork/WeatherReader.git 
   and send sensors value at Weather Underground site  */

        int main(char **args) {


        MYSQL_RES *result;
        MYSQL_ROW row;
        MYSQL *connection, mysql;

        int state;
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

        char *datetime;
                datetime = malloc( 20 * sizeof(char) );

        char *temperature;
                temperature = malloc( 10 * sizeof(char) );

        char *firstrain;
                firstrain = malloc( 10 * sizeof(char) );

        char *lastrain;
                lastrain = malloc( 10 * sizeof(char) );

        char *hourlastrain;
                hourlastrain = malloc( 10 * sizeof(char) );

        char *humidity;
                humidity = malloc( 10 * sizeof(char) );
                
        char *pressure;
                pressure = malloc( 10 * sizeof(char) );

        char *wind_speed;
                wind_speed = malloc( 10 * sizeof(char) );

        char *wind_gust;
                wind_gust = malloc( 10 * sizeof(char) );

        char *wind_dir;
                wind_dir = malloc( 10 * sizeof(char) );

        mysql_init(&mysql);
        connection = mysql_real_connect(&mysql,"localhost", "user", "passwd", 
                                    "database", 0, 0, 0);

        if (connection == NULL) {
                printf("%s", mysql_error(&mysql));
                return 1;
        }
        
        /* Selection temperature */
        
	state = mysql_query(connection,
                "SELECT time, value FROM wr_temperature order by time desc limit 1");

        if (state != 0) {
                printf("%s", mysql_error(connection));
                return 1;
        }

        result = mysql_store_result(connection);
           
        while ( ( row = mysql_fetch_row(result)) != NULL ) {
                strcpy(datetime, (row[0] ? row[0] : "NULL"));
                strcpy(temperature, (row[1] ? row[1] : "NULL"));
        }

         mysql_free_result(result);
         
         /* Selection humidity */
         
	state = mysql_query(connection,
                "SELECT value FROM wr_humidity order by time desc limit 1");

        if (state != 0) {
                printf("%s", mysql_error(connection));
                return 1;
        }
        result = mysql_store_result(connection);
            
        while ( ( row = mysql_fetch_row(result)) != NULL ) {
                strcpy(humidity, (row[0] ? row[0] : "NULL"));

        }

        mysql_free_result(result);

	/* selection barometric pressure */

        state = mysql_query(connection,
                "SELECT value FROM wr_barometer order by time desc limit 1");

        if (state != 0) {
                printf("%s", mysql_error(connection));
                return 1;
        }
        
        result = mysql_store_result(connection);

        while ( ( row = mysql_fetch_row(result)) != NULL ) {
                strcpy(pressure, (row[0] ? row[0] : "NULL"));
        }

        mysql_free_result(result);
        
        /* selection rain dayly total*/
        
	state = mysql_query(connection,
                "select total from wr_rain where date(time) = date (curdate()) order by time desc limit 1;");

        if (state != 0) {
                printf("%s", mysql_error(connection));
                return 1;
        }

        result = mysql_store_result(connection);
               
        while ( ( row = mysql_fetch_row(result)) != NULL ) {
                strcpy(lastrain, (row[0] ? row[0] : "NULL"));
        }

        mysql_free_result(result);
	
	/* Select rain 1 hour */

	state = mysql_query(connection,
              "select total from wr_rain where date(time) = date (curdate()) order by time limit 1;");

        if (state != 0) {
                printf("%s", mysql_error(connection));
                return 1;
        }

        result = mysql_store_result(connection);
                
        while ( ( row = mysql_fetch_row(result)) != NULL ) {
                strcpy(firstrain, (row[0] ? row[0] : "NULL"));
        }

        mysql_free_result(result);

        state = mysql_query(connection,
                "select total, time from wr_rain where timestamp(time) >= date_sub(now(), interval 60 minute)  limit 1");
	if (state != 0) {
                printf("%s", mysql_error(connection));
                return 1;
        }

        result = mysql_store_result(connection);

        while ( ( row = mysql_fetch_row(result)) != NULL ) {
                strcpy(hourlastrain, (row[0] ? row[0] : "NULL"));
        }

        mysql_free_result(result);

	/* Select wind data */
	
        state = mysql_query(connection,
                "SELECT speed, gust, dir FROM wr_wind order by time desc limit 1");

        if (state != 0) {
                printf(mysql_error(connection)); 
                return 1;
        }

        result = mysql_store_result(connection);
               
        while ( ( row = mysql_fetch_row(result)) != NULL ) {
                strcpy(wind_speed, (row[0] ? row[0] : "NULL"));
                strcpy(wind_gust,  (row[1] ? row[1] : "NULL"));
                strcpy(wind_dir,   (row[2] ? row[2] : "NULL"));
        }

         mysql_free_result(result);

/* End mysql data retrieved */

        ftemperature = atof(temperature);
        fhumidity = atof(humidity);
        ffirstrain = atof(firstrain);
        flastrain = atof(lastrain);
        fhourlastrain = atof(hourlastrain);
	fwind_speed = atof(wind_speed);
        fwind_gust = atof(wind_gust);
        fwind_dir = atof(wind_dir);
        fpressure = atof(pressure);

        diffrain = flastrain - ffirstrain;
        diffhourrain = flastrain - fhourlastrain;
        
	printf("\nTEMP:%.1f\nHUMIDITY:%.0f\nRAIN:%.2f\nRAIN HOUR:%.2f\nWind Speed:%.1f\nWind Gust:%.1f\nWind Direction:%.0f\nPressure: %.0f\n",
                ftemperature, fhumidity, diffrain, diffhourrain, fwind_speed, fwind_gust, fwind_dir, fpressure);
        

        wa_send_data(ftemperature, fhumidity, diffrain, diffhourrain, fwind_speed, fwind_gust, fwind_dir, fpressure);

        /* close the connection */
         mysql_close(connection);
}
