#include "llama.h"
#include <string>
#include <vector>
#include "gsl.hpp"
#include <fstream>
#include <iterator>
#include <iostream>

int main() {
  std::string model_name = "/home/adam/data1/ollama-dl/library-qwen2.5-0.5b/model-c5396e06af29.gguf";
  // std::string model_name = "/home/adam/data1/ollama-dl/library-llama3.2-1b-instruct-fp16/model-1ec85b6f7ac0.gguf";
  // std::string model_name = "/home/adam/data1/ollama-dl/library-llama3.2-1b/model-74701a8c35f6.gguf";

  std::ifstream prompt_file{"/home/adam/data1/work/nifi-minifi-cpp/llama-test/llama-prompt.txt", std::ios::binary};
  std::string prompt{std::istreambuf_iterator<char>{prompt_file}, std::istreambuf_iterator<char>{}};

  std::string msg =
      "attributes:\n"
      "  uuid: 7\n"
      "  source: llama.cpp\\n:78\n"
      "content:\n"
      "  This is a great line\n";

  llama_backend_init();

  llama_model_params model_params = llama_model_default_params();
  auto* model = llama_load_model_from_file(model_name.c_str(), model_params);

  llama_context_params ctx_params = llama_context_default_params();
  auto* ctx = llama_new_context_with_model(model, ctx_params);

  const int n_ctx_train = llama_n_ctx_train(model);
  const int n_ctx = llama_n_ctx(ctx);
  const int n_batch = llama_n_batch(ctx);

  gsl_Assert(n_ctx <= n_ctx_train);

  std::vector<std::pair<std::string, std::string>> examples;
  examples.emplace_back(
    "attributes:\n"
    "  uuid: 1234\n"
    "  filename: index.txt\n"
    "content:\n"
    "  This product is crap\n",

    "attributes:\n"
    "  uuid: 1234\n"
    "  filename: index.txt\n"
    "  sentiment: negative\n"
    "content:\n"
    "  This product is crap\n"
    "relationship:\n"
    "  Negative\n"
  );

  examples.emplace_back(
    "attributes:\n"
    "  uuid: 4548\n"
    "  date: 2024.01.01.\n"
    "content:\n"
    "  What a wonderful day\n",

    "attributes:\n"
    "  uuid: 4548\n"
    "  date: 2024.01.01.\n"
    "  sentiment: positive\n"
    "content:\n"
    "  What a wonderful day\n"
    "relationship:\n"
    "  Positive\n"
  );

  std::string input = [&] {
    std::vector<llama_chat_message> msgs;
    msgs.push_back(llama_chat_message{.role = "system", .content = prompt.c_str()});
    for (auto& [input, output] : examples) {
      msgs.push_back(llama_chat_message{.role = "user", .content = input.c_str()});
      msgs.push_back(llama_chat_message{.role = "assistant", .content = output.c_str()});
    }
    msgs.push_back(llama_chat_message{.role = "user", .content = msg.c_str()});

    std::string text;
    int32_t res_size = llama_chat_apply_template(model, nullptr, msgs.data(), msgs.size(), true, text.data(), text.size());
    if (res_size > text.size()) {
      text.resize(res_size);
      llama_chat_apply_template(model, nullptr, msgs.data(), msgs.size(), true, text.data(), text.size());
    }
    text.resize(res_size);

    return text;
  }();

  std::cout << "Full input prompt:\n" << input << std::endl;

  std::vector<llama_token> enc_input = [&] {
    int32_t n_tokens = input.length() + 2;
    std::vector<llama_token> enc_input(n_tokens);
    n_tokens = llama_tokenize(model, input.data(), input.length(), enc_input.data(), enc_input.size(), true, true);
    if (n_tokens < 0) {
      enc_input.resize(-n_tokens);
      int check = llama_tokenize(model, input.data(), input.length(), enc_input.data(), enc_input.size(), true, true);
      gsl_Assert(check == -n_tokens);
    } else {
      enc_input.resize(n_tokens);
    }
    return enc_input;
  }();

  gsl_Assert(!enc_input.empty());

  auto sparams = llama_sampler_chain_default_params();
  llama_sampler* smpl = llama_sampler_chain_init(sparams);

  llama_sampler_chain_add(smpl, llama_sampler_init_top_k(50));
  llama_sampler_chain_add(smpl, llama_sampler_init_top_p(0.9, 1));
  llama_sampler_chain_add(smpl, llama_sampler_init_temp (0.8));
  llama_sampler_chain_add(smpl, llama_sampler_init_dist(1234));

  llama_batch batch = llama_batch_get_one(enc_input.data(), enc_input.size(), 0, 0);
  int n_pos = 0;

  llama_token new_token_id;

  while (true) {
    if (int32_t res = llama_decode(ctx, batch); res < 0) {
      throw std::logic_error("failed to execute decode");
    }
    n_pos += batch.n_tokens;

    new_token_id = llama_sampler_sample(smpl, ctx, -1);

    if (llama_token_is_eog(model, new_token_id)) {
      break;
    }

    llama_sampler_accept(smpl, new_token_id);

    std::array<char, 128> buf;
    int32_t len = llama_token_to_piece(model, new_token_id, buf.data(), buf.size(), 0, true);
    if (len < 0) {
      throw std::logic_error("failed to convert to text");
    }
    gsl_Assert(len < 128);
    buf[len] = '\0';

    printf("%s", buf.data());
    fflush(stdout);

    batch = llama_batch_get_one(&new_token_id, 1, n_pos, 0);
  }

  llama_sampler_free(smpl);
  llama_free(ctx);
  llama_free_model(model);

  llama_backend_free();

  return 0;
}