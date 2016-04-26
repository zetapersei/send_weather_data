
/*************************************************************************
   send_data.c

   This modules retrieve data from Mysql database and send data  with http post
   This application use data stored by https://github.com/hassebjork/WeatherReader.git 
   and send sensors value at Weather Underground site.
   
  
   
   The source code can be found at GitHub 
   
   
**************************************************************************

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

*************************************************************************/





#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <mysql.h>

        static const char URL_DATA[] = "http://weatherstation.wunderground.com/weatherstation/updateweatherstation.php";
        static const char ID_DATA[] = "station_id";
        static const char PASSWD_DATA[] = "user_password";

        /* Function implementation */

        void wa_send_data ( float CTemp, float humid, float day_amount, float windg, float winddir) {

                CURL *curl;
                CURLcode res;

                float FTemp = CTemp*9/5 + 32;

                /* Dew Point simple formula: proposed in a 2005 article by Mark G. Lawrence in the Bulletin of the American Meteorological Society:
                        Td = T - ((100 - RH)/5.)
                        Another formula is:
                        a = 17.27
                        b = 237.7
                        gamma = (a * t / (b + t)) + ln(rh / 100.0)
                        dew = b * gamma / (a - gamma)*/

                float degree_dewpt = CTemp - ((100 - humid)/5);
                float dewptf = degree_dewpt*9/5 + 32;
                /* float inc_amount = (amount - 72.25)/25.445;*/
                float incday_amount = day_amount / 25.445;
                float windgmph = (windg * 2.236);
                 /* float inch_pressure = pressure/33.865;*/

                char *postdata;
                        postdata = malloc(300 * sizeof(char));
			
      /* Used information available at site: http://wiki.wunderground.com/index.php/PWS_-_Upload_Protocol#Background*/

                sprintf(postdata,"ID=%s&PASSWORD=%s&dateutc=now&tempf=%.1f&humidity=%.1f&dewptf=%.1f&dailyrainin=%.3f&windgustmph=%.2f&winddir=%.0f&action=updateraw",
                        ID_DATA, PASSWD_DATA, FTemp, humid, dewptf, incday_amount, windgmph, winddir);

                curl = curl_easy_init();

                if(curl) {
                        curl_easy_setopt(curl, CURLOPT_URL, URL_DATA);
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(postdata));
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);

                        res = curl_easy_perform(curl);
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

}
/* Retrieve data from Mysql database and send data  with http post
   This application use data stored by https://github.com/hassebjork/WeatherReader.git 
   and send sensors value at Weather Underground site  */

        int main() {


        MYSQL_RES *result;
        MYSQL_ROW row;
        MYSQL *connection, mysql;

        int state;
        float ftemperature;
        float fhumidity;
        float ffirstrain;
        float flastrain;
        float fwind_speed;
        float fwind_gust;
        float fwind_dir;
        float diffrain;

        char *datetime;
                datetime = malloc( 20 * sizeof(char) );

        char *temperature;
                temperature = malloc( 10 * sizeof(char) );

        char *firstrain;
                firstrain = malloc( 10 * sizeof(char) );

        char *lastrain;
                lastrain = malloc( 10 * sizeof(char) );

        char *humidity;
                humidity = malloc( 10 * sizeof(char) );

        char *wind_speed;
                wind_speed = malloc( 10 * sizeof(char) );


	char *wind_gust;
                wind_gust = malloc( 10 * sizeof(char) );

        char *wind_dir;
                wind_dir = malloc( 10 * sizeof(char) );

        /* database initialization */

        mysql_init(&mysql);
        connection = mysql_real_connect(&mysql,"localhost", "user", "password", 
                                    "weather", 0, 0, 0);

        if (connection == NULL) {
        
                printf("%s", mysql_error(&mysql));
                return 1;
        }

        /* Retrieve temperature data */
        
	state = mysql_query(connection,
                "SELECT time, value FROM wr_temperature order by time desc limit 1");

        if (state != 0) {
                printf("%s", mysql_error(connection));
	}

        result = mysql_store_result(connection);
            
        while ( ( row = mysql_fetch_row(result)) != NULL ) {
                strcpy(datetime, (row[0] ? row[0] : "NULL"));
                strcpy(temperature, (row[1] ? row[1] : "NULL"));
        }

         mysql_free_result(result);

	 /* Retrieve humidity data */
        
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

        /* Retrieve rain data for daily rain total */

        state = mysql_query(connection,
                "select total from wr_rain where date(time) = date (curdate()) order by time desc limit 1;");

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

	/* Retrieve wind data */

        state = mysql_query(connection,
                "SELECT speed, gust, dir FROM wr_wind order by time desc limit 1");

        if (state != 0) {
                printf("%s", mysql_error(connection));
                return 1;
        }

        result = mysql_store_result(connection);          

        while ( ( row = mysql_fetch_row(result)) != NULL ) {
                strcpy(wind_speed, (row[0] ? row[0] : "NULL"));
                strcpy(wind_gust,  (row[1] ? row[1] : "NULL"));
                strcpy(wind_dir,   (row[2] ? row[2] : "NULL"));
        }

         mysql_free_result(result);
	 
	/* Data conversion and send the data */ 

        ftemperature = atof(temperature);
        fhumidity = atof(humidity);
        ffirstrain = atof(firstrain);
        flastrain = atof(lastrain);
        fwind_speed = atof(wind_speed);
        fwind_gust = atof(wind_gust);
        fwind_dir = atof(wind_dir);

	diffrain = flastrain - ffirstrain;

        printf("DATA:%s\nTEMP:%.1f\nHUMIDITY:%.0f\nRAIN:%.0f\nWind Speed:%.1f\nWind Gust:%.1f\nWind Direction:%.0f\n", 
                datetime, ftemperature, fhumidity, diffrain, fwind_speed, fwind_gust, fwind_dir);

       /* wa_send_data(ftemperature, fhumidity, diffrain, fwind_gust, fwind_dir);*/

        /* close database connection */
         mysql_close(connection);
	 
}