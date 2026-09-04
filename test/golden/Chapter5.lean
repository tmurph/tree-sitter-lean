import Rudin.Basic

/-!
# Consequences of Baire's Theorem (5.5)
-/

open scoped Topology

variable {X : Type*}
variable [TopologicalSpace X]
variable {ι : Type*}

-- okay I want to start fresh, but also, I think I have an elegant
-- strategy that doesn't match Rudin. What's going on here?

-- prove for two first
lemma dense_inter {V W : Set X} (OpenV : IsOpen V)
                  (DenseV : Dense V) (DenseW : Dense W)
    : Dense (V ∩ W) := by
  rw [dense_iff_inter_open]
  intro U OpenU SomeU
  have OpenUV := OpenU.inter OpenV
  have SomeUV := DenseV.inter_open_nonempty U OpenU SomeU
  have SomeUVW := DenseW.inter_open_nonempty _ OpenUV SomeUV
  rwa [← Set.inter_assoc]

-- now for finitely many
lemma Finite.dense_iInter [Finite ι] {V : ι → Set X} (OpenV : ∀ i, IsOpen (V i))
                          (DenseV : ∀ i, Dense (V i))
    : Dense (⋂ i, V i) := by
  induction ι using Finite.induction_empty_option with
  | of_equiv e ih =>
    have : ⋂ b, V b = ⋂ a, (V ∘ e) a := by
      refine Set.iInter_congr_of_surjective e.symm e.symm.surjective fun b ↦ ?_
      rw [Function.comp_apply, Equiv.apply_symm_apply]
    exact this ▸ @ih (V ∘ e) (OpenV <| e ·) (DenseV <| e ·)
  | h_empty =>
    have : ⋂ b, V b = Set.univ := Set.iInter_of_empty V
    exact this ▸ dense_univ
  | h_option ih =>
    have : ⋂ b, V b = (V none) ∩ ⋂ a, (V ∘ some) a := Set.iInter_option V
    have ih' := @ih (V ∘ some) (OpenV <| some ·) (DenseV <| some ·)
    exact this ▸ dense_inter (OpenV none) (DenseV none) ih'

-- this approach doesn't feel as promising anymore.  I guess that's why
-- rudin doesn't do it.  but still, surely we don't need metric space
-- assumptions to prove it, right?
theorem t_5_6_baire [Countable ι] [Uncountable X]
                    {V : ι → Set X} (OpenV : ∀ i, IsOpen (V i))
                    (DenseV : ∀ i, Dense (V i))
    : Dense (⋂ i, V i) := by
  intro x W ⟨ClosedW, iInter_sub_W⟩
  exact Set.mem_of_mem_of_subset (rfl : x ∈ {x}) <|
    calc {x}
    _ ⊆ ⋂ i, closure (V i) := by
      intro x rfl _ ⟨i, Vi⟩
      exact Vi ▸ DenseV i x
    _ ⊆ closure (⋂ i, V i) := sorry
    _ ⊆ W := ClosedW.closure_subset_iff.mpr iInter_sub_W

section Baire

variable [BaireSpace X]

-- c.f. BaireSpace definition
example {V : ℕ → Set X} (ho : ∀ i, IsOpen (V i)) (hd : ∀ i, Dense (V i))
    : Dense (⋂ i, V i) :=
  BaireSpace.baire_property V ho hd

-- also their version that swaps ℕ to just "countable"
example [Countable ι] {f : ι → Set X} (ho : ∀ i, IsOpen (f i))
        (hd : ∀ i, Dense (f i))
    : Dense (⋂ s, f s) := dense_iInter_of_isOpen ho hd

end Baire
