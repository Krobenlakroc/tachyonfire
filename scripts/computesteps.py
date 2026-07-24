import math


def solve_ratio(s, M, D_rem, tol=1e-9, max_iter=100):
    """
    Solve:
        s * (r^M - 1) / (r - 1) = D_rem
    for r >= 1
    """

    if M == 1:
        return 1.0  # single step case

    # Lower bound (no growth)
    lo = 1.0
    hi = 2.0

    # Expand upper bound until large enough
    def f(r):
        return s * (r**M - 1.0) / (r - 1.0)

    while f(hi) < D_rem:
        hi *= 2.0

    # Binary search
    for _ in range(max_iter):
        mid = 0.5 * (lo + hi)
        val = f(mid)

        if abs(val - D_rem) < tol:
            return mid

        if val < D_rem:
            lo = mid
        else:
            hi = mid

    return 0.5 * (lo + hi)


def generate_steps(total_distance, total_steps, linear_steps, linear_step_size):
    if linear_steps > total_steps:
        raise ValueError("linear_steps cannot exceed total_steps")

    s = linear_step_size
    L = linear_steps
    N = total_steps
    M = N - L

    linear_distance = L * s

    if linear_distance > total_distance:
        raise ValueError("Linear steps exceed total distance")

    D_rem = total_distance - linear_distance

    # If no exponential section
    if M == 0:
        return [s] * L

    # Solve for growth ratio
    r = solve_ratio(s, M, D_rem)

    # Build step list
    steps = [s] * L
    for i in range(M):
        steps.append(s * (r ** i))

    # Small correction for floating point
    correction = total_distance - sum(steps)
    steps[-1] += correction

    return steps


# Example
if __name__ == "__main__":
    total_distance = 7000.0
    total_steps = 28
    linear_steps = 5
    linear_step_size = 142.0

    steps = generate_steps(total_distance, total_steps, linear_steps, linear_step_size)
    # int iters[] = {6,4,6};
    # print("Step deltas:")
    # for s in steps:
    #     print(f"{s:.6f}")
    print("float deltas[] = {",end="")
    for s in steps:
        print(f"{s:.6f},",end="")
    print("};",end="")

    print("\nTotal distance:", sum(steps))
