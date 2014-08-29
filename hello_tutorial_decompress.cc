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
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/websocket.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "lzo/minilzo.h"
#include "ppapi/c/pp_errors.h"       // std::string
// swapping ostringstream objects

#include <string>       // std::string
#include <mutex>          // std::mutex
#include <stack>          // std::stack
#include <thread>




/* We want to compress the data block at 'in' with length 'IN_LEN' to
 * the block at 'out'. Because the input block may be incompressible,
 * we must provide a little more output space in case that compression
 * is not possible.
 */



/* Work-memory needed for compression. Allocate memory in units
 * of 'lzo_align_t' (instead of 'char') to make sure it is properly aligned.
 */

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
    for(int i = 0; i < 75; i++){
      pp::WebSocket* const skt = new pp::WebSocket(this);
      skt->Connect(pp::Var("ws://localhost:8081/prj2/websocket/a"), NULL, 0, factory.NewCallback(&HelloTutorialInstance::push_socket, skt));
    }
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
          std::thread(&HelloTutorialInstance::start_web_socket, this, 0, var_message.AsString()).detach();
  }

  
private:
  pp::CompletionCallbackFactory<HelloTutorialInstance> factory;
  std::mutex mtx;           // mutex for critical section
  std::stack<pp::WebSocket*> Socket_Stack;

  void push_socket(int32_t r, pp::WebSocket* skt){
    Socket_Stack.push(skt);
  }

  void start_web_socket(int32_t r, const std::string& f_name){
        pp::WebSocket *skt;
        mtx.lock();
        if(Socket_Stack.size() <= 0){
          mtx.unlock();
          skt = new pp::WebSocket(this);
          if(skt->Connect(pp::Var("ws://localhost:8081/prj2/websocket/a"), NULL, 0, pp::BlockUntilComplete()) == PP_OK){
            on_connect(0, skt, f_name);  
          }else{
            PostMessage(f_name + ":ERROR:");      
            delete skt;
          }
        }else{  
          skt = Socket_Stack.top();
          Socket_Stack.pop();
          mtx.unlock();
          on_connect(0, skt, f_name);
        }
  }

  void on_connect(int32_t z, pp::WebSocket* const skt, const std::string& f_name){
    pp::VarArrayBuffer file_name = pp::VarArrayBuffer(f_name.length());
    pp::Var *output = new pp::Var();
    
    std::memcpy(static_cast<void*>(file_name.Map()), f_name.c_str(), f_name.length());
    skt->SendMessage(file_name);
    if(skt->ReceiveMessage(output, pp::BlockUntilComplete()) == PP_OK){
      on_recieve(0, output, skt, f_name);
    }else{
      PostMessage(f_name + ":ERROR:");
      skt->Close(PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, pp::Var(), pp::BlockUntilComplete());
      delete skt;
    }
    file_name.Unmap();
    delete output;
  }
  
  void on_recieve(int32_t z, pp::Var * const message, pp::WebSocket* const skt, const std::string& f_name){
        lzo_uint old_len;  
        lzo_uint new_len;

        mtx.lock();
        Socket_Stack.push(skt);
        mtx.unlock();

        unsigned char* const decompressed = new unsigned char[259271 + 259271 / 16 + 64 + 3];
        old_len = static_cast<pp::VarArrayBuffer* const>(message)->ByteLength();
        const unsigned char* const data  = static_cast<const unsigned char * const>(static_cast<pp::VarArrayBuffer*const>(message)->Map());
        if(lzo1x_decompress(data,old_len,decompressed,&new_len,NULL) == LZO_E_OK){
          PostMessage(pp::Var(f_name + std::string(":OK:") + std::string((const char * const)decompressed)));
        }else{
          PostMessage(f_name + ":ERROR:");
        }
        
        static_cast<pp::VarArrayBuffer*>(message)->Unmap();
        delete decompressed;
        //skt.Close(PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, pp::Var(), pp::BlockUntilComplete());
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
