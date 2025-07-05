import json

# 파일 경로 설정
#original_dataset_path = "../../arc_challenge.json"  # 원본 데이터셋 파일 경로
#results_path = "../../Arc-Challenge_mllm_Qwen1.5_1.8B_result.json"        # LLM 추론 결과 파일 경로
original_dataset_path = "../../arc_easy.json"  # 원본 데이터셋 파일 경로
results_path = "../../Arc-Easy_mllm_Qwen1.5_1.8B_result.json"        # LLM 추론 결과 파일 경로


# JSON 파일 로드 함수
def load_json(path):
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)

# 질문 목록 추출 함수
def extract_questions_from_original(data):
    # 원본 데이터셋에는 각 항목에 'instruction' 키가 있다고 가정
    return {item['instruction'] for item in data}


def extract_questions_from_results(data):
    # 추론 결과 파일의 각 항목에는 'question' 키가 있다고 가정
    return {item['question'] for item in data}


def find_missing_questions(orig_questions, result_questions):
    # 원본에는 있지만 결과에는 없는 질문을 반환
    return orig_questions - result_questions


def main():
    # 파일 로드
    original_data = load_json(original_dataset_path)
    results_data = load_json(results_path)

    # 질문 집합 생성
    orig_questions = extract_questions_from_original(original_data)
    result_questions = extract_questions_from_results(results_data)
    print(f"Loaded Questions: Original {len(orig_questions)} | Target: {len(result_questions)}")

    # 누락된 질문 찾기
    missing = find_missing_questions(orig_questions, result_questions)

    # 결과 출력
    if missing:
        print("원본 데이터셋에 존재하지만, 결과 파일에 없는 질문들:")
        for q in sorted(missing):
            print(q)
    else:
        print("모든 질문이 결과 파일에 포함되어 있습니다.")

if __name__ == "__main__":
    main()