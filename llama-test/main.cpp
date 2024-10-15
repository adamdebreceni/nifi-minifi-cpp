#include "llama.h"
#include <string>
#include <vector>
#include "gsl.hpp"

int main() {
  std::string model_name = "/Users/adebreceni/work/minifi-homes/llama-test/asset/model-74701a8c35f6.gguf";

  std::string prompt =
      "You are a helpful assistant or otherwise called an AI processor.\n"
      "You are part of a flow based pipeline helping the user transforming and routing data (encapsulated in what is called flowfiles).\n"
      "The user will provide the data, it will have attributes (name and value) and a content.\n"
      "The output route is also called a relationship.\n"
      "You should only output the transformed flowfiles and a relationships to be transferred to.\n"
      "You might produce multiple flowfiles if instructed.\n"
      "You get $10000 if you respond according to the expected format.\n"
      "Do not use any other relationship than what the specified ones.\n"
      "Only split flow files when it is explicitly requested.\n"
      "An example interaction follows: \n"
      "Reflect on what relationships you are allowed to use, and do not write your internal reasoning only the output.\n"
      "Example user input:"
      "<attribute-name>uuid</attribute-name>\n"
      "<attribute-value>1234</attribute-value>\n"
      "<attribute-name>filename</attribute-name>\n"
      "<attribute-value>index.txt</attribute-value>\n"
      "<content>Hello World</content>\n"
      "Expected expected answer for the previous input:\n"
      "<attribute-name>uuid</attribute-name>\n"
      "<attribute-value>2</attribute-value>\n"
      "<content>Hello</content>\n"
      "<relationship>Success</relationship>\n"
      "<attribute-name>new-attr</attribute-name>\n"
      "<attribute-value>new-val</attribute-value>\n"
      "<content>Planet</content>\n"
      "<relationship>Other</relationship>\n"
      "\n\n"
      "What now follows is a description of how the user would like you to transform/route their data, and what relationships you are allowed to use:\n"
      "Determine if the sentiment is positive or negative routing to relationships Positive or Negative respectively.";


  std::string msg =
      "<attribute-name>uuid</attribute-name>\n"
      "<attribute-value>7</attribute-value>\n"
      "<content>This is a great product</content>\n";

  llama_backend_init();

  llama_model_params model_params = llama_model_default_params();
  auto* model = llama_load_model_from_file(model_name.c_str(), model_params);

  llama_context_params ctx_params = llama_context_default_params();
  auto* ctx = llama_new_context_with_model(model, ctx_params);

  const int n_ctx_train = llama_n_ctx_train(model);
  const int n_ctx = llama_n_ctx(ctx);
  const int n_batch = llama_n_batch(ctx);

  gsl_Assert(n_ctx <= n_ctx_train);

  std::string input = [&] {
    std::vector<llama_chat_message> msgs;
    msgs.push_back(llama_chat_message{.role = "system", .content = prompt.c_str()});
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