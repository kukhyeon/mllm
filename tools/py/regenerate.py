from datasets import load_dataset
from datasets import concatenate_datasets

import json

arc_challenge = load_dataset("ai2_arc", "ARC-Challenge")

data = concatenate_datasets([arc_challenge['test'], arc_challenge["train"], arc_challenge['validation']])

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

js = list()

for i in range(len(data)):
    ins, out, ans_key = make_prompt(data[i])
    js.append({"instruction":ins, "output":out, "answerKey": ans_key})

with open("arc_challenge.json", 'w') as f:
    json.dump(js, f, indent=4)