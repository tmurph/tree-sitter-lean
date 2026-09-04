import Rudin.Basic

/-!
# Exercises
-/

open Filter
open MeasureTheory
open Pointwise
open Set

open scoped ENNReal
open scoped Topology

variable {X : Type*}
variable [TopologicalSpace X]
variable {ι : Type*}

/- Suppose that `E` is a measurable set of real numbers with arbitrarily
  small periods.  Prove that either `E` or its complement has measure 0. -/
lemma ex_3 {E : Set ℝ} {ε : ℕ → ℝ} {μ : Measure ℝ}
           (periodic : ∀ i, E + {ε i} = E)
           (to_zero : Tendsto ε atTop (𝓝 0)) (pos : ∀ i, ε i > 0)
    : μ E = 0 ∨ (μ Eᶜ) = 0 := by
  have α : ℝ := default
  let F : ℝ → ℝ≥0∞ := fun x ↦ μ (E ∩ Icc α x)
  have hint
    : ∀ x y : ℝ, ∀ i : ℕ, α + ε i < x ∧ x < y →
      F (x + ε i) - F (x - ε i) = F (y + ε i) - F (y - ε i) := by
    done
  done

-- consider this?
example {μ : Measure ℝ} {E : Set ℝ} {p : ℝ → Prop} [NoAtoms μ]
    (h : ∀ a b, a ∈ E → b ∈ E → a < b → ∀ᵐ x ∂μ, x ∈ E ∩ Ioo a b → p x)
    : ∀ᵐ x ∂μ, x ∈ E → p x := ae_of_mem_of_ae_of_mem_inter_Ioo h
