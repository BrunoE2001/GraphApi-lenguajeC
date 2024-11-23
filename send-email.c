#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/stat.h>

// Definir la configuración para obtener token
// DECLARAR las credenciales de .env

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

// Función para codificar en Base64
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
char *base64_encode(const unsigned char *data, size_t input_length, size_t *output_length)
{
    *output_length = 4 * ((input_length + 2) / 3);

    char *encoded_data = (char *)malloc(*output_length + 1);
    if (encoded_data == NULL)
    {
        fprintf(stderr, "No se pudo asignar memoria para la codificación Base64.\n");
        exit(1);
    }

    for (size_t i = 0, j = 0; i < input_length;)
    {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        encoded_data[j++] = base64_table[(triple >> 18) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 12) & 0x3F];
        encoded_data[j++] = (i > input_length + 1) ? '=' : base64_table[(triple >> 6) & 0x3F];
        encoded_data[j++] = (i > input_length) ? '=' : base64_table[triple & 0x3F];
    }

    encoded_data[*output_length] = '\0';
    return encoded_data;
}

// Leer archivo y codificar en Base64
char *read_file_base64(const char *filepath)
{
    printf("Entro a la funcion read_file_base64\n\n");
    FILE *file = fopen(filepath, "rb");
    if (!file)
    {
        fprintf(stderr, "Error: No se pudo abrir el archivo %s\n", filepath);
        exit(1);
    }

    // Obtener tamaño del archivo
    struct stat st;
    if (stat(filepath, &st) != 0)
    {
        fprintf(stderr, "Error al obtener el tamaño del archivo %s\n", filepath);
        fclose(file);
        exit(1);
    }
    size_t filesize = st.st_size;

    // Leer el archivo en un buffer
    unsigned char *buffer = (unsigned char *)malloc(filesize);
    if (!buffer)
    {
        fprintf(stderr, "Error: No se pudo asignar memoria para el archivo.\n");
        fclose(file);
        exit(1);
    }

    fread(buffer, 1, filesize, file);
    fclose(file);

    // Codificar en Base64
    printf("Entro a la funcion read_file_base64 - Codificar en Base64\n\n");
    size_t encoded_length;
    char *encoded = base64_encode(buffer, filesize, &encoded_length);

    printf("Retorna el encoded\n\n");
    free(buffer);
    return encoded;
}

// Construir y enviar el correo
void send_email(const char *recipientEmail, const char *recipientName, const char *subject, const char *htmlContent, const char **filePaths, int fileCount, char *token)
{
    printf("Entro a la funcion send-email\n\n");
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    char url[512];
    char *payload = NULL;
    char *attachments = NULL;

    // Construir la URL de la API
    printf("Construir la URL de la API\n\n");
    snprintf(url, sizeof(url), "%s", ENDPOINT);

    // Leer y codificar archivos adjuntos
    printf("Leer y codificar archivos adjuntos\n\n");
    attachments = malloc(1);
    attachments[0] = '\0';
    for (int i = 0; i < fileCount; i++)
    {
        printf("FOR read_file_base64\n\n");
        const char *filePath = filePaths[i];
        char *fileBase64 = read_file_base64(filePath);

        // Calcular el nuevo tamaño necesario
        printf("Calcular el nuevo tamano necesario\n\n");
        size_t new_size = strlen(attachments) + strlen(fileBase64) + 256;
        attachments = realloc(attachments, new_size);
        if (!attachments)
        {
            fprintf(stderr, "Error: No se pudo asignar memoria para los adjuntos.\n");
            free(fileBase64);
            return;
        }

        // Agregar el adjunto al JSON
        printf("Agregar el adjunto al JSON\n\n");
        // Obtener el nombre del archivo de manera segura
        printf("Obtener el nombre del archivo de manera segura\n\n");
        const char *filename = strrchr(filePath, '/'); // Para sistemas basados en UNIX
        if (!filename)
            filename = strrchr(filePath, '\\'); // Para Windows
        if (!filename)
            filename = filePath; // Si no hay separador, usar el path completo
        else
            filename++; // Saltar el separador para obtener solo el nombre

        // Asegurar suficiente espacio en el buffer
        printf("Asegurar suficiente espacio en el buffer\n\n");
        size_t newsize = strlen(attachments) + strlen(fileBase64) + strlen(filename) + 256;
        attachments = realloc(attachments, newsize);
        if (!attachments)
        {
            fprintf(stderr, "Error: No se pudo asignar memoria para los adjuntos.\n");
            free(fileBase64);
            return;
        }

        // Concatenar el contenido del archivo al JSON
        printf("Concatenar el contenido del archivo al JSON\n\n");
        strcat(attachments, "{ \"contentType\": \"");
        strcat(attachments, strstr(filePath, ".pdf") ? "application/pdf" : "image/jpeg");
        strcat(attachments, "\", \"name\": \"");
        strcat(attachments, filename);
        strcat(attachments, "\", \"contentInBase64\": \"");
        strcat(attachments, fileBase64);
        strcat(attachments, "\" }");

        printf("Adjunto agregado exitosamente: %s\n\n", filename);

        free(fileBase64);
    }

    // Construir el cuerpo del correo
    printf("Construir el cuerpo del correo\n\n");
    size_t payload_size = strlen(attachments) + 1024;
    payload = malloc(payload_size);
    if (!payload)
    {
        fprintf(stderr, "Error: No se pudo asignar memoria para el cuerpo del correo.\n");
        free(attachments);
        return;
    }

    printf("JSON BODY: %s\n %s\n %s\n %s\n %s\n\n", SENDER_EMAIL, subject, htmlContent, recipientEmail, recipientName);
    snprintf(payload, payload_size,
             "{"
             "  \"senderAddress\": \"%s\","
             "  \"content\": {"
             "    \"subject\": \"%s\","
             "    \"html\": \"%s\""
             "  },"
             "  \"recipients\": {"
             "    \"to\": [{ \"address\": \"%s\" }] }"
             "}",
             SENDER_EMAIL, subject, htmlContent, recipientEmail, recipientName);
    fprintf(stderr, "PAYLOAD: %s\n", payload);
    // Inicializar cURL
    printf("Inicializar cURL\n\n");
    curl = curl_easy_init();
    if (!curl)
    {
        fprintf(stderr, "Error inicializando cURL.\n");
        free(payload);
        free(attachments);
        return;
    }

    // Configurar los encabezados
    printf("Configurar los encabezados\n\n");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    char authHeader[256];
    snprintf(authHeader, sizeof(authHeader), "Authorization: Bearer %s", token);
    headers = curl_slist_append(headers, authHeader);
    fprintf(stderr, "HEADER: %s\n\n", headers);

    // Configurar cURL
    printf("Configurar cURL\n\n");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);

    // Realizar la solicitud
    printf("Realizar la solicitud\n\n");
    res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        fprintf(stderr, "Error en la solicitud cURL: %s\n", curl_easy_strerror(res));
    }
    else
    {
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != 200)
        {
            // Imprimir la respuesta completa de la API para obtener más detalles sobre el error
            char *response_data;
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
            printf("Error al enviar el correo. Código de respuesta: %ld\nRespuesta: %s\n", response_code, response_data);
        }
        else
        {
            printf("Correo enviado exitosamente.\n");
        }
    }

    // Liberar recursos
    printf("Recursos liberados\n\n");
    free(payload);
    free(attachments);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Uso: %s <numero de correo>\n", argv[0]);
        return 1;
    }

    CURL *curl;
    CURLcode res;
    struct response api_response = {0};

    // Datos para obtener el token
    char post_data[512];
    snprintf(post_data, sizeof(post_data),
             "grant_type=client_credentials&client_id=%s&client_secret=%s&scope=%s",
             CLIENT_ID, CLIENT_SECRET, SCOPE);

    // Inicializar cURL para obtener el token
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, TOKEN_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &api_response);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "Error al obtener el token: %s\n", curl_easy_strerror(res));
            return 1;
        }

        // Analizar la respuesta JSON y extraer el token
        char *token_start = strstr(api_response.data, "\"access_token\":\"");
        if (token_start)
        {
            token_start += 16; // Desplazarse al inicio del token
            char *token_end = strchr(token_start, '"');
            if (token_end)
            {
                *token_end = '\0'; // Terminar la cadena del token
                printf("Token obtenido exitosamente: Authorization: Bearer %s\n\n\n", token_start);

                // Enviar el correo
                const char *file_paths[] = {"data.pdf", "ASEG5-eMail Sender - Level 1.jpg"};
                send_email("capinemo940@gmail.com", "CapitanNemo", "Solicitud de Documento Adjuntado",
                           "<h1>Estimado Capitan Nemo,</h1><p>Le enviamos el documento solicitado en el archivo adjunto.</p>",
                           file_paths, 2, token_start);
            }
        }
        else
        {
            fprintf(stderr, "Error: No se encontró el token en la respuesta.\n");
        }

        // Limpiar memoria de la respuesta
        free(api_response.data);
        curl_easy_cleanup(curl);
    }
    else
    {
        fprintf(stderr, "Error al inicializar cURL.\n");
        return 1;
    }

    curl_global_cleanup();
    return 0;
}
