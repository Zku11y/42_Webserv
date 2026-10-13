#include "../../inc/main.hpp"

// int ft_handle_get(Client& client)
// {

//     return 0;
// }

// int ft_handle_post(Client& client)
// {

//     return 0;
// }

// int ft_handle_delet(Client& client)
// {

//     return 0;
// }

void ft_handle_resource(Client& client,Config&  config)
{
    // if(client.parsed_req.method=="GET")
    // {
    //     ft_handle_get(client);
    //     return;
    // }
    // else if(client.parsed_req.method=="POST")
    // {
    //     ft_handle_post(client);
    //     return;
    // }
    // else if(client.parsed_req.method=="DELET")
    // {
    //     ft_handle_delet(client);
    //     return;
    // }
    
    
    ft_set_error(client, 200);
    return;
}



















// // ============================================================
// // DEV 3: RESOURCE HANDLERS
// //
// // These are placeholders for your implementation.
// // They do not build HTTP responses.
// // Each handler should produce a resource result in Client.
// // ============================================================

// static void ft_serve_file(Client& client)
// {
//     // TODO:
//     // Read client.parsed_req.physical_path.
//     // Set client.raw_body and client.status_code.
//     ft_set_error(client, 501);
// }

// static void ft_handle_directory(Client& client)
// {
//     // TODO:
//     // 1. Resolve the configured index file, if applicable.
//     // 2. If no index exists and autoindex is enabled,
//     //    generate a directory listing.
//     // 3. Otherwise return the appropriate error.
//     ft_set_error(client, 501);
// }

// static void ft_handle_upload(Client& client)
// {
//     // TODO:
//     // Implement upload handling using the request method,
//     // body and configured upload_store.
//     ft_set_error(client, 501);
// }

// static void ft_handle_delete(Client& client)
// {
//     // TODO:
//     // Implement DELETE after validating the resolved path
//     // and the request's configured method permissions.
//     ft_set_error(client, 501);
// }

// static void ft_start_cgi(Client& client)
// {
//     // Implement this function in cgi.cpp.
//     // Execute the configured interpreter and script.
//     // Return the CGI result through the agreed Client fields.
//     ft_set_error(client, 501);
// }

// // ============================================================
// // DEV 3: MAIN ENTRY POINT
// //
// // This is the main entry point for your entire part.
// //
// // Input:
// //   client.parsed_req
// //
// // Output:
// //   client.raw_body
// //   client.status_code
// //
// // Dev 2 formats the final HTTP response.
// // Dev 1 sends that response to the client.
// // ============================================================

// static void ft_handle_resource(Client& client)
// {
//     const HttpRequest& req = client.parsed_req;

//     client.raw_body.clear();
//     client.status_code = 200;

//     // 1. Validate the request/configuration handoff.
//     if (req.method.empty() || req.physical_path.empty())
//     {
//         ft_set_error(client, 400);
//         return;
//     }

//     // 2. Dispatch according to the requested operation.
//     //
//     // CGI takes priority when a CGI interpreter is configured.
//     // Upload and DELETE handling are selected by method.
//     // Directory handling is selected by the resolved resource.
//     // Remaining requests are treated as static-file requests.

//     if (!req.cgi_executable.empty())
//     {
//         if (req.method != "GET" && req.method != "POST")
//         {
//             ft_set_error(client, 405);
//             return;
//         }

//         ft_start_cgi(client);
//         return;
//     }

//     if (req.method == "DELETE")
//     {
//         ft_handle_delete(client);
//         return;
//     }

//     if (req.method == "POST" && !req.upload_store.empty())
//     {
//         ft_handle_upload(client);
//         return;
//     }

//     if (req.is_directory)
//     {
//         ft_handle_directory(client);
//         return;
//     }

//     if (req.method == "GET" || req.method == "HEAD")
//     {
//         ft_serve_file(client);
//         return;
//     }

//     ft_set_error(client, 405);
// }