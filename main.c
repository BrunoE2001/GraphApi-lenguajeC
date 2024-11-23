#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define CLIENT_ID "1d602504-b9e3-4b98-ae7b-488d4dd873ee"
#define CLIENT_SECRET "LMZ8Q~eBfEyNATE1KUQSyBtiA5Ol08~wY1WwmcvR"
#define TENANT_ID "48992719-6127-4a26-8ca6-88201ba83b75"
#define SCOPE "https%3A%2F%2Fgraph.microsoft.com%2F.default"
#define TOKEN_URL "https://login.microsoftonline.com/48992719-6127-4a26-8ca6-88201ba83b75/oauth2/v2.0/token"

// Estructura para almacenar la respuesta del token
struct response
{
    char *data;
    size_t size;
};

// Función para escribir la respuesta de la API en una estructura
size_t write_callback(void *ptr, size_t size, size_t nmemb, struct response *res)
{
    size_t new_size = res->size + size * nmemb;
    res->data = realloc(res->data, new_size + 1);
    if (res->data == NULL)
    {
        printf("No se pudo asignar memoria\n");
        return 0;
    }
    memcpy(res->data + res->size, ptr, size * nmemb);
    res->data[new_size] = '\0';
    res->size = new_size;
    return size * nmemb;
}

int main()
{
    CURL *curl;
    CURLcode res;
    struct response api_response = {0};

    // Datos para obtener el token
    char post_data[512];
    snprintf(post_data, sizeof(post_data),
             "grant_type=client_credentials&client_id=%s&client_secret=%s&scope=%s",
             CLIENT_ID, CLIENT_SECRET, SCOPE);

    // Configurar la solicitud POST para obtener el token
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl)
    {
        // Establecer la URL de la solicitud
        curl_easy_setopt(curl, CURLOPT_URL, TOKEN_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

        // Establecer los encabezados necesarios para la solicitud
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Establecer el callback para la respuesta
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &api_response);

        // Ejecutar la solicitud
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() falló: %s\n", curl_easy_strerror(res));
        }
        else
        {
            // Imprimir la respuesta de la API (que contiene el token)
            printf("Respuesta de la API: %s\n", api_response.data);
        }

        // Limpiar
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    free(api_response.data);

    return 0;
}
