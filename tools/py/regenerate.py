from datasets import load_dataset
from datasets import concatenate_datasets
import numpy as np

import json

def sample_dataset(data, num_samples=1000):
    sampled = None
    if len(data) < num_samples:
        sampled = [i for i in range(len(data))]
    else:
        sampled = np.random.choice(np.arange(0, len(data)), size=num_samples, replace=False)

    return sampled

def regenerate_arc_challenge():
    def make_prompt(dic):
        question = dic['question']
        answer_text = dic['choices']['text']
        answer_label = dic['choices']['label']
        answer_key = dic['answerKey']

        instruction = f'{question}'
        output = ''
        for i in range(len(answer_label)):
            instruction += f'\n\t{answer_label[i]}. {answer_text[i]}'
            if answer_label[i] == answer_key:
                output += f'{answer_key}. {answer_text[i]}'
    
        return  instruction, output, answer_key
    
    
    arc_challenge = load_dataset("ai2_arc", "ARC-Challenge")
    data = arc_challenge['test']
    sampled = sample_dataset(data, num_samples=1000)

    js = list()

    for i in sampled:
        ins, out, ans_key = make_prompt(data[i])
        js.append({"instruction":ins, "output":out, "answerKey": ans_key})

    with open("dataset/arc_challenge.json", 'w') as f:
        json.dump(js, f, indent=4)


def regenerate_winogrande():
    def make_prompt(dic):
        sentence = dic['sentence']
        options = (dic['option1'], dic['option2'])
        answer_key = dic['answer']
        
        instruction = f'In the sentence: "{sentence}", which option correctly completes the sentence?\n\t1. {options[0]}\n\t2. {options[1]}'
        output = ''
        if answer_key == '1':
            output = f'1. {options[0]}'
        else:
            output = f'2. {options[1]}'
        
        return instruction, output, answer_key
    

    winogrande = load_dataset("allenai/winogrande", "winogrande_l")
    data = concatenate_datasets([winogrande['validation']]) # winogrande does not have answer key in test set
    
    sampled = sample_dataset(data, num_samples=1000)

    for i in sampled:
        print(make_prompt(data[i]))
    print(f"sampled {len(sampled)} items.")

    js = list()

    # data store
    for i in range(len(data)):
        ins, out, ans_key = make_prompt(data[i])
        js.append({"instruction":ins, "output":out, "answerKey": ans_key})

    with open("dataset/winogrande.json", 'w') as f:
        json.dump(js, f, indent=4)


def regenerate_humaneval():
    def make_prompt(dic):
        instruction = dic['prompt']
        output = dic['canonical_solution']
        answer_key = dic['test']
        
        return instruction, output, answer_key
    

    humaneval = load_dataset("openai_humaneval")
    data = humaneval['test']

    sampled = sample_dataset(data, num_samples=1000)

    js = list()

    for i in sampled[:3]:
        print(make_prompt(data[i]))
        

    # data store
    for i in sampled:
        ins, out, ans_key = make_prompt(data[i])
        js.append({"instruction":ins, "output":out, "answerKey": ans_key})

    with open("dataset/humaneval.json", 'w') as f:
        json.dump(js, f, indent=4)

def regenerate_xsum():
    def make_prompt(dic):
        instruction = f"Summarize the following article in only one sentence:\n\n{dic['document']}"
        output = dic['summary']
        return instruction, output, ''
    
    xsum = load_dataset("EdinburghNLP/xsum", revision="refs/convert/parquet", split="test")
    data = xsum['test']
    sampled = sample_dataset(data, num_samples=1000)
    
    js = list()

    for i in sampled:
        ins, out, ans_key = make_prompt(data[i])
        js.append({"instruction": ins, "output": out, "answerKey": ans_key})

    with open("dataset/xsum.json", 'w') as f:
        json.dump(js, f, indent=4)

if __name__ == "__main__":
    # regenerate_arc_challenge()
    # regenerate_winogrande()
    # regenerate_humaneval()
    regenerate_xsum()