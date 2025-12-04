import re
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def load_worker_deltas(path="worker_deltas.txt"):
    """worker_deltas.txt에서 데이터와 DVFS 요청 시각을 읽는다."""
    dvfs_times = []

    # 먼저 헤더에서 dvfs_req_time_ns[...] 주석을 파싱
    with open(path, "r") as f:
        header_lines = []
        data_lines = []
        for line in f:
            if line.startswith("#"):
                header_lines.append(line.strip())
            else:
                data_lines.append(line)
        # dvfs_req_time_ns[...] 파싱
        pattern = re.compile(r"dvfs_req_time_ns\[(\d+)\]\s*=\s*([0-9.eE+-]+)")
        for h in header_lines:
            m = pattern.search(h)
            if m:
                idx = int(m.group(1))
                t_val = float(m.group(2))
                dvfs_times.append((idx, t_val))

    # 데이터 부분은 pandas로 읽기
    from io import StringIO
    buffer = StringIO("".join(data_lines))
    df = pd.read_csv(
        buffer,
        delim_whitespace=True,
        header=None,
        names=["t_ns", "dt_us"],
    )

    return df, dvfs_times


def analyze_jitter(df, sigma_k=5.0):
    """dt_us에서 '유독 긴 dt' 스파이크를 찾아 평균/STD를 출력."""
    dt = df["dt_us"].values

    # 1) 전체 기본 통계
    overall_mean = dt.mean()
    overall_std = dt.std()
    print(f"[Overall] mean={overall_mean:.6f} us, std={overall_std:.6f} us")

    # 2) baseline 추정 (상위 1% 제외한 값으로)
    cutoff = np.percentile(dt, 99)  # 상위 1%는 일단 스파이크 후보로 제거
    baseline = dt[dt < cutoff]

    base_mean = baseline.mean()
    base_std = baseline.std()
    print(f"[Baseline(<99%)] mean={base_mean:.6f} us, std={base_std:.6f} us")

    # 3) baseline 평균 + k * std 이상을 스파이크로 간주
    threshold = base_mean + sigma_k * base_std
    spikes = dt[dt >= threshold]
    spike_indices = np.where(dt >= threshold)[0]

    print(f"[Spike detection] threshold={threshold:.6f} us "
          f"(k={sigma_k:.1f} * baseline std)")
    print(f"  # of spikes = {len(spikes)} / {len(dt)}")

    if len(spikes) > 0:
        spike_mean = spikes.mean()
        spike_std = spikes.std()
        print(f"[Spikes] mean={spike_mean:.6f} us, std={spike_std:.6f} us")
        print(f"  max spike dt = {spikes.max():.6f} us")
    else:
        print("[Spikes] no spikes detected (with this threshold).")

    # 4) 간단한 시각화
    plt.figure()
    plt.plot(df["t_ns"], df["dt_us"], ".", markersize=1)
    plt.scatter(
        df["t_ns"].iloc[spike_indices],
        df["dt_us"].iloc[spike_indices],
        marker="o",
        s=10,
    )
    plt.xlabel("time (ns)")
    plt.ylabel("delta (us)")
    plt.title("dt_us over time (spikes highlighted)")
    plt.tight_layout()
    plt.show()

    return {
        "overall_mean": overall_mean,
        "overall_std": overall_std,
        "baseline_mean": base_mean,
        "baseline_std": base_std,
        "threshold": threshold,
        "spike_mean": spikes.mean() if len(spikes) > 0 else None,
        "spike_std": spikes.std() if len(spikes) > 0 else None,
        "num_spikes": int(len(spikes)),
        "num_samples": int(len(dt)),
        "spike_indices": spike_indices,
    }


if __name__ == "__main__":
    df, dvfs_times = load_worker_deltas("worker_deltas.txt")
    print("DVFS req times (index, t_ns):")
    for idx, t in dvfs_times:
        print(f"  [{idx}] t_ns = {t:.0f}")

    stats = analyze_jitter(df, sigma_k=5.0)