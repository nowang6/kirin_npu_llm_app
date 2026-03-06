## context.json
```json
{
    "version": 1,
    "engine_type": "autoregressive",
    "generate_options": {
        "callback_freq": 2,
        "max_gen_tokens": 600,
        "stop_sequence": ["<|im_end|>"],
        "init_token_len": 0
    },
    "sampler": {
        "do_sample": true,
        "seed": 99,
        "top-k": 16,
        "top-p": 0.95,
        "temperature": 0.6,
        "repetition_penalty": 1.2
    }
}
```

## execute.json
### Qwen3 1.7B
```json
{
    "version": 1,
    "engine_type": "autoregressive",
    "llm_config" : {
        "bos_token_id": 151643,
        "eos_token_id": 151645,
        "kv_cache_max_len": 2048,
        "sliding_window_len": 0,
        "max_position_embeddings": 40960,
        "num_attention_kv_heads": 8,
        "num_attention_head_dims": 128,
        "num_hidden_layers": 28,
        "prefill_len": 64,
        "decode_len": 1,
        "vocab_size": 151936,
        "vocab_real_size": 151936,
        "use_output_pos": false,
        "max_io_tokens": 4096,
        "hidden_size": 2048,
        "embedding_weights": "model_64_2048.embedding_weights",
        "embedding_dequant_scale": "model_64_2048.embedding_dequant_scale",
        "embedding_input_type": "int8"
    },
    "tokenizer": {
        "type": "qwen",
        "path": "/data/storage/el2/base/files/tokenizer.json"
    },
    "autoregressive": {
        "model_path": "/data/storage/el2/base/files/omc_out.omc",
        "weight_path": "/data/storage/el2/base/files"
    }
}
```

### Qwen2 0.5B
```json
{
    "version": 1,
    "engine_type": "autoregressive",
    "llm_config" : {
        "bos_token_id": 151643,
        "eos_token_id": 151643,
        "kv_cache_max_len": 2048,
        "sliding_window_len": 0,
        "max_position_embeddings": 32768,
        "num_attention_kv_heads": 2,
        "num_attention_head_dims": 64,
        "num_hidden_layers": 24,
        "prefill_len": 64,
        "decode_len": 1,
        "vocab_size": 151936,
        "vocab_real_size": 151936,
        "use_output_pos": false,
        "max_io_tokens": 4096,
        "hidden_size": 896,
        "embedding_weights": "model_64_2048.embedding_weights",
        "embedding_dequant_scale": "model_64_2048.embedding_dequant_scale",
        "embedding_input_type": "int8"
    },
    "tokenizer": {
        "type": "qwen",
        "path": "/data/storage/el2/base/files/tokenizer.json"
    },
    "autoregressive": {
        "model_path": "/data/storage/el2/base/files/omc_out.omc",
        "weight_path": "/data/storage/el2/base/files"
    }
}
```

## 部署
```sh
hdc file send executor.json /data/app/el2/100/base/com.huawei.cannkit.llmengine/files/

hdc file send context.json /data/app/el2/100/base/com.huawei.cannkit.llmengine/files/

hdc file send model_64_2048.embedding_dequant_scale /data/app/el2/100/base/com.huawei.cannkit.llmengine/files/

hdc file send model_64_2048.embedding_weights /data/app/el2/100/base/com.huawei.cannkit.llmengine/files

hdc file send omc_out.omc /data/app/el2/100/base/com.huawei.cannkit.llmengine/files/

hdc file send SubGraph_0.weight /data/app/el2/100/base/com.huawei.cannkit.llmengine/files/

hdc file send tokenizer.json /data/app/el2/100/base/com.huawei.cannkit.llmengine/files/

```

