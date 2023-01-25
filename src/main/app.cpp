#include "app.hpp"
#include <spdlog/spdlog.h>
#include "parser.hpp"
#include "websocket.hpp"

namespace application {

AppLayer::AppLayer(const AppOptions& options, network::HttpSender& sender, network::ProtocolUpgrader& upgrader)
    : options{options}, sender{sender}, upgrader{upgrader} {
}

void AppLayer::Process(network::HttpRequest&& req) {
  spdlog::debug("app received request {} {} {}", req.method, req.uri, req.version);
  if (req.uri == "/ws") {
    ServeWebsocket(req);
    return;
  }
  ServeFile(req);
}

void AppLayer::ServeWebsocket(const network::HttpRequest& req) {
  network::WebsocketHandshakeBuilder handshakeBuilder{req};
  auto resp = handshakeBuilder.Build();
  if (not resp) {
    network::HttpResponse notFoundResp;
    notFoundResp.status = network::HttpStatus::NotFound;
    notFoundResp.headers.emplace("Content-Type", "text/plain");
    notFoundResp.body = "Not Found";
    sender.Send(std::move(notFoundResp));
    return;
  }
  sender.Send(std::move(*resp));
  upgrader.UpgradeToWebsocket();
}

void AppLayer::ServeFile(const network::HttpRequest& req) {
  std::string uri = req.uri;
  if (uri.ends_with("/")) {
    uri += "index.html";
  }
  if (uri.starts_with("/")) {
    uri.erase(0, 1);
  }
  char localPathStr[PATH_MAX];
  realpath(uri.c_str(), localPathStr);
  std::string localPath{localPathStr};
  spdlog::debug("request local path is {}", localPath);
  if (not localPath.starts_with(options.wwwRoot)) {
    network::HttpResponse resp;
    resp.status = network::HttpStatus::NotFound;
    resp.headers.emplace("Content-Type", "text/plain");
    resp.body = "Not Found";
    return sender.Send(std::move(resp));
  }
  std::string mimeType = MimeTypeOf(localPath);
  network::FileHttpResponse resp;
  resp.path = std::move(localPath);
  resp.headers.emplace("Content-type", std::move(mimeType));
  return sender.Send(std::move(resp));
}

std::string AppLayer::MimeTypeOf(std::string_view path) {
  if (path.ends_with(".html")) {
    return "text/html";
  }
  return "text/plain";
}

AppLayerFactory::AppLayerFactory(const AppOptions& options_) : options{options_} {
  const std::string s{options.wwwRoot};
  char resolvedPath[PATH_MAX];
  realpath(s.c_str(), resolvedPath);
  chdir(resolvedPath);
  options.wwwRoot = resolvedPath;
}

std::unique_ptr<network::HttpProcessor> AppLayerFactory::Create(
    network::HttpSender& sender, network::ProtocolUpgrader& upgrader) const {
  return std::make_unique<AppLayer>(options, sender, upgrader);
}

}  // namespace application
