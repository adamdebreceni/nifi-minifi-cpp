/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "AiProcessor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "Resource.h"
#include "Exception.h"

namespace org::apache::nifi::minifi::processors {

namespace {

struct LlamaChatMessage {
  std::string role;
  std::string content;

  operator llama_chat_message() const {
    return llama_chat_message{
      .role = role.c_str(),
      .content = content.c_str()
    };
  }
};

//constexpr const char* relationship_prompt = R"(You are a helpful assistant helping to analyze the user's description of a data transformation and routing algorithm.
//The data consists of attributes and a content encapsulated in what is called a flowfile.
//The routing targets are called relationships.
//You have to extract the comma separated list of all possible relationships one can route to based on the user's description.
//Output only the list and nothing else.
//)";

}  // namespace

void AiProcessor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void AiProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  context.getProperty(ModelName, model_name_);
  context.getProperty(Prompt, prompt_);

  full_prompt_ =
      "You are a helpful assistant or otherwise called an AI processor.\n"
      "You are part of a flow based pipeline helping the user transforming and routing data (encapsulated in what is called flowfiles).\n"
      "The user will provide the data, it will have attributes (name and value) and a content.\n"
      "The output route is also called a relationship.\n"
      "You should only output the transformed flowfiles and a relationships to be transferred to.\n"
      "You might produce multiple flowfiles if instructed.\n"
      "An example interaction follows: \n"
      "User input:"
      "<attribute-name>uuid</attribute-name>\n"
      "<attribute-value>1234</attribute-value>\n"
      "<attribute-name>filename</attribute-name>\n"
      "<attribute-value>index.txt</attribute-value>\n"
      "<content>Hello World</content>\n"
      "Expected answer:\n"
      "<attribute-name>uuid</attribute-name>\n"
      "<attribute-value>2</attribute-value>\n"
      "<content>Hello</content>\n"
      "<relationship>Success</relationship>\n"
      "<attribute-name>new-attr</attribute-name>\n"
      "<attribute-value>new-val</attribute-value>\n"
      "<content>Planet</content>\n"
      "<relationship>Other</relationship>\n"
      "\n\n"
      "What now follows is a description of how the user would like you to transform/route their data, and what relationships you are allowed to use:\n" + prompt_;

  llama_backend_init();

  llama_model_params model_params = llama_model_default_params();
  llama_model_ = llama_load_model_from_file(model_name_.c_str(), model_params);

  llama_context_params ctx_params = llama_context_default_params();
  llama_ctx_ = llama_new_context_with_model(llama_model_, ctx_params);

  sampler_ = gpt_sampler_init(llama_model_, sparams);

//  {
//    // load relationships
//    std::vector<llama_chat_message> messages;
//    messages.push_back(llama_chat_message{.role = "system", .content = relationship_prompt});
//    messages.push_back(llama_chat_message{.role = "user", .content = prompt_.c_str()});
//
//
//    int32_t res_size = llama_chat_apply_template(llama_model_, nullptr, messages.data(), messages.size(), true, nullptr, 0);
//    std::string buf;
//    buf.resize(res_size);
//    llama_chat_apply_template(llama_model_, nullptr, messages.data(), messages.size(), true, buf.data(), buf.size());
//
//    auto relationships = utils::string::splitAndTrim(buf, ",");
//
//  }
}

void AiProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto input = session.get();
  if (!input) {
    context.yield();
    return;
  }

  auto read_result = session.readBuffer(input);

  std::vector<LlamaChatMessage> messages;
  messages.push_back(LlamaChatMessage{.role = "system", .content = full_prompt_});
  std::string input_data;
  for (auto& [name, val] : input->getAttributes()) {
    input_data += "<attribute-name>" + name + "</attribute-name>\n";
    input_data += "<attribute-value>" + val + "</attribute-value>\n";
  }
  input_data += "<content>" + std::string{reinterpret_cast<const char*>(read_result.buffer.data()), read_result.buffer.size()} + "</content>\n";
  messages.push_back(LlamaChatMessage{.role = "user", .content = input_data});

  int32_t res_size = 0;
  for (auto& msg : messages) {
    res_size += (msg.role.size() + msg.content.size()) * 1.25;
  }

  std::vector<llama_chat_message> native_messages(messages.begin(), messages.end());

  std::string text;
  text.resize(res_size);

  res_size = llama_chat_apply_template(llama_model_, nullptr, native_messages.data(), native_messages.size(), true, text.data(), text.size());
  if (res_size > text.size()) {
    text.resize(res_size);
    llama_chat_apply_template(llama_model_, nullptr, native_messages.data(), native_messages.size(), true, text.data(), text.size());
  }
  text.resize(res_size);

  int32_t n_tokens = text.length() + 2;
  std::vector<llama_token> enc_input(n_tokens);
  n_tokens = llama_tokenize(llama_model_, text.data(), text.length(), enc_input.data(), enc_input.size(), true, true);
  if (n_tokens < 0) {
    enc_input.resize(-n_tokens);
    int check = llama_tokenize(llama_model_, text.data(), text.length(), enc_input.data(), enc_input.size(), true, true);
    gsl_Assert(check == -n_tokens);
  } else {
    enc_input.resize(n_tokens);
  }

  if (llama_model_has_encoder(llama_model_)) {
    if (int32_t res = llama_encode(llama_ctx_, llama_batch_get_one(enc_input.data(), enc_input.size(), 0, 0)); res != 0) {
      throw Exception(PROCESSOR_EXCEPTION, "Failed to execute encoder");
    }

    llama_token decoder_start_token_id = llama_model_decoder_start_token(llama_model_);
    if (decoder_start_token_id == -1) {
      decoder_start_token_id = llama_token_bos(llama_model_);
    }

    enc_input.clear();
    enc_input.push_back(decoder_start_token_id);
  }


  std::string_view output = text;

  while (!output.empty()) {
    auto result = session.create();
    auto rest = output;
    while (output.starts_with("<attribute-name>")) {
      output = output.substr(std::strlen("<attribute-name>"));
      auto name_end = output.find("</attribute-name>");
      if (name_end != std::string_view::npos) {
        auto name = output.substr(0, name_end);
        output = output.substr(name_end + std::strlen("</attribute-name>"));
        if (output.starts_with("<attribute-value>")) {
          output = output.substr(std::strlen("<attribute-value>"));
          auto val_end = output.find("</attribute-value>");
          if (val_end != std::string_view::npos) {
            auto val = output.substr(0, val_end);
            output = output.substr(val_end + std::strlen("</attribute-value>"));
            result->setAttribute(name, std::string{val});
            continue;
          }
        }
      }
      // failed to parse attributes, dump the rest as malformed
      session.writeBuffer(result, rest);
      session.transfer(result, Malformed);
      return;
    }
    if (!output.starts_with("<content>")) {
      // no content
      session.writeBuffer(result, rest);
      session.transfer(result, Malformed);
      return;
    }
    output = output.substr(std::strlen("<content>"));
    auto content_end = output.find("</content>");
    if (content_end == std::string_view::npos) {
      // no content closing tag
      session.writeBuffer(result, rest);
      session.transfer(result, Malformed);
      return;
    }
    auto content = output.substr(0, content_end);
    output = output.substr(content_end + std::strlen("</content>"));
    if (!output.starts_with("<relationship>")) {
      // no relationship opening tag
      session.writeBuffer(result, rest);
      session.transfer(result, Malformed);
      return;
    }
    auto rel_end = output.find("</relationship>");
    if (rel_end == std::string_view::npos) {
      // no relationship closing tag
      session.writeBuffer(result, rest);
      session.transfer(result, Malformed);
      return;
    }
    auto rel = output.substr(0, rel_end);
    output = output.substr(rel_end + std::strlen("</relationship>"));

    session.transfer(result, core::Relationship{std::string{rel}, ""});
  }
}

void AiProcessor::notifyStop() {
  llama_free(llama_ctx_);
  llama_ctx_ = nullptr;
  llama_free_model(llama_model_);
  llama_model_ = nullptr;
  llama_backend_free();
}

REGISTER_RESOURCE(AiProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
