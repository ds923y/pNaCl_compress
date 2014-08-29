// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// @file hello_tutorial.cc
/// This example demonstrates loading, running and scripting a very simple NaCl
/// module.  To load the NaCl module, the browser first looks for the
/// CreateModule() factory method (at the end of this file).  It calls
/// CreateModule() once to load the module code.  After the code is loaded,
/// CreateModule() is not called again.
///
/// Once the code is loaded, the browser calls the CreateInstance()
/// method on the object returned by CreateModule().  It calls CreateInstance()
/// each time it encounters an <embed> tag that references your NaCl module.
///
/// The browser can talk to your NaCl module via the postMessage() Javascript
/// function.  When you call postMessage() on your NaCl module from the browser,
/// this becomes a call to the HandleMessage() method of your pp::Instance
/// subclass.  You can send messages back to the browser by calling the
/// PostMessage() method on your pp::Instance.  Note that these two methods
/// (postMessage() in Javascript and PostMessage() in C++) are asynchronous.
/// This means they return immediately - there is no waiting for the message
/// to be handled.  This has implications in your program design, particularly
/// when mutating property values that are exposed to both the browser and the
/// NaCl module.


#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/websocket.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/utility/threading/simple_thread.h"
#include <boost/config/compiler/clang.hpp>
#include <boost/uuid/sha1.hpp>
#include "lzo/minilzo.h"
#include <cstring>
// swapping ostringstream objects
#include <string>       // std::string
#include <iostream>     // std::cout
#include <sstream>      // std::stringstream
#include <memory>




/* We want to compress the data block at 'in' with length 'IN_LEN' to
 * the block at 'out'. Because the input block may be incompressible,
 * we must provide a little more output space in case that compression
 * is not possible.
 */



/* Work-memory needed for compression. Allocate memory in units
 * of 'lzo_align_t' (instead of 'char') to make sure it is properly aligned.
 */

#define HEAP_ALLOC(var,size) \
    lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

namespace {
	// The expected string sent by the browser.
	const char* const kHelloString = "hello";
	// The string sent back to the browser upon receipt of a message
	// containing "hello".
	const char* const kReplyString = "hello from NaCl";

  //static unsigned char __LZO_MMODEL compressed  [ 100000 ];
} // namespace
/// The Instance class.  One of these exists for each instance of your NaCl
/// module on the web page.  The browser will ask the Module object to create
/// a new Instance for each occurrence of the <embed> tag that has these
/// attributes:
///     src="hello_tutorial.nmf"
///     type="application/x-pnacl"
/// To communicate with the browser, you must override HandleMessage() to
/// receive messages from the browser, and use PostMessage() to send messages
/// back to the browser.  Note that this interface is asynchronous.
class HelloTutorialInstance : public pp::Instance {
 public:
  /// The constructor creates the plugin-side instance.
  /// @param[in] instance the handle to the browser-side plugin instance.
  //, rcv_thread_(this)
  explicit HelloTutorialInstance(PP_Instance instance) : pp::Instance(instance), factory(this)
  {

        if (lzo_init() != LZO_E_OK) {
          PostMessage(pp::Var("failed to load"));
        }
  }
  virtual ~HelloTutorialInstance() {}

  
  /// Handler for messages coming in from the browser via postMessage().  The
  /// @a var_message can contain be any pp:Var type; for example int, string
  /// Array or Dictinary. Please see the pp:Var documentation for more details.
  /// @param[in] var_message The message posted by the browser.
  virtual void HandleMessage(const pp::Var& var_message) {

	  if (!var_message.is_string())
		  return;
        start_web_socket(var_message.AsString());
  }

  
private:
  pp::CompletionCallbackFactory<HelloTutorialInstance> factory;

  void start_web_socket(const std::string& to_server){
        std::shared_ptr<pp::WebSocket> skt(new pp::WebSocket(this));
        //pp::WebSocket *skt = new pp::WebSocket(this);
        skt->Connect(pp::Var("ws://localhost:8081/prj2/websocket/b"), NULL, 0, factory.NewCallback(&HelloTutorialInstance::on_connect, skt, to_server));
  }

  void on_connect(int32_t z, const std::shared_ptr<pp::WebSocket> skt, const std::string& to_server){
    lzo_uint out_len = 100000;
    unsigned char* compressed = new unsigned char[259271 + 259271 / 16 + 64 + 3];
    PostMessage(pp::Var("compressing"));
    int r = lzo1x_1_compress((const unsigned char *)to_server.c_str(),to_server.length(),compressed,&out_len,wrkmem);
    PostMessage(pp::Var("done compressing"));
    pp::VarArrayBuffer to_server_bin = pp::VarArrayBuffer(out_len);
    unsigned char* compressed_container = static_cast<unsigned char *>(to_server_bin.Map());

    std::memcpy(static_cast<void *>(compressed), compressed_container, out_len);
    skt->SendMessage(to_server_bin);
    to_server_bin.Unmap();
    pp::Var *output = new pp::Var();
    skt->ReceiveMessage(output, factory.NewCallback(&HelloTutorialInstance::on_recieve, output, skt));
  }
  
  void on_recieve(int32_t z, pp::Var *message, std::shared_ptr<pp::WebSocket> skt){
        PostMessage(*message);
        //skt->Close(PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, pp::Var(), factory.NewCallback(&HelloTutorialInstance::no_op));
        delete message;
  }

  void no_op(int32_t n){}

  __attribute__((unused)) std::string get_sha1(char *will_hash, int new_len){
        boost::uuids::detail::sha1 s;
        //std::string a = "Franz jagt im komplett verwahrlosten Taxi quer durch Bayern";
        s.process_bytes(will_hash, new_len);
        unsigned int digest[5];
        s.get_digest(digest);
        std::stringstream ss;
        for(int i=0; i<5; ++i)
          ss << std::hex << digest[i];
        return ss.str();
  }
};

/// The Module class.  The browser calls the CreateInstance() method to create
/// an instance of your NaCl module on the web page.  The browser creates a new
/// instance for each <embed> tag with type="application/x-pnacl".
class HelloTutorialModule : public pp::Module {
 public:
  HelloTutorialModule() : pp::Module() {}
  virtual ~HelloTutorialModule() {}

  /// Create and return a HelloTutorialInstance object.
  /// @param[in] instance The browser-side instance.
  /// @return the plugin-side instance.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new HelloTutorialInstance(instance);
  }
};

namespace pp {
/// Factory function called by the browser when the module is first loaded.
/// The browser keeps a singleton of this module.  It calls the
/// CreateInstance() method on the object you return to make instances.  There
/// is one instance per <embed> tag on the page.  This is the main binding
/// point for your NaCl module with the browser.
Module* CreateModule() {
  return new HelloTutorialModule();
}
}  // namespace pp
