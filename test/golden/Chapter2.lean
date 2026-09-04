import Rudin.Basic

open Set
open scoped Topology

variable {X : Type*} [TopologicalSpace X]
variable {K F : Set X}

inductive Extends (ι : Type u) where
  | extra : Extends ι
  | original (i : ι) : Extends ι

lemma t_2_4 (hK : IsCompact K) (hF : IsClosed F) (hsub : F ⊆ K) : IsCompact F := by
  refine isCompact_of_finite_subcover ?_
  intro ι V hOpen hFV
  let V' (i' : Extends ι) : Set X := match i' with
  | .extra => Fᶜ
  | .original i => V i
  have hOpen' (i' : Extends ι) : IsOpen (V' i') := match i' with
  | .extra => hF.isOpen_compl
  | .original i => hOpen i
  have : Set.univ ⊆ ⋃ i', V' i' := by
    intro x _
    by_cases hx : x ∈ F
    · obtain ⟨i, hi⟩ := mem_iUnion.1 (hFV hx)
      exact mem_iUnion.2 ⟨.original i, hi⟩
    · exact mem_iUnion.2 ⟨.extra, hx⟩
  have hKV' : K ⊆ ⋃ i', V' i' := K.subset_univ.trans this
  obtain ⟨t', hKt'⟩ := hK.elim_finite_subcover V' hOpen' hKV'
  use t'.preimage Extends.original (fun _ _ _ _ => Extends.original.inj)
  intro x hxF
  obtain ⟨i', hi't', hxi'⟩ := mem_iUnion₂.mp (hKt' (hsub hxF))
  cases i' with
  | extra => contradiction -- hxF == x ∈ F, hxi' == x ∈ Fᶜ
  | original i =>
  exact mem_iUnion₂.mpr ⟨i, Finset.mem_preimage.mpr hi't', hxi'⟩

-- for comparison, here's the same theorem in Mathlib.  they prove it
-- with filters and cluster points.
example (hK : IsCompact K) (hF : IsClosed F) (hsub : F ⊆ K) : IsCompact F :=
  IsCompact.of_isClosed_subset hK hF hsub

section Hausdorff

variable [T2Space X]

-- This feels like a case where the filter argument should be easier?
-- But I don't know that api at all. Maybe it'll come with time.
--
-- Yeah like, the first step in Rudin is to grab hold of a finite set of
-- points whose neighborhoods cover K. Mathlib provides a lot of lemmas
-- for that, but all of them are phrased in terms of neighborhood
-- filters. Joys. I'm really not going to get into that just yet.
lemma t_2_5 {p : X} (hK : IsCompact K) (hpK : p ∈ Kᶜ)
    : ∃ U V, IsOpen U ∧ IsOpen V ∧ K ⊆ U ∧ p ∈ V ∧ Disjoint U V := by
  have H (q : K) := t2_separation <| ne_of_mem_of_not_mem q.property hpK
  choose U V hU hV hqU hpV hUV using H
  have : K ⊆ ⋃ i, U i := fun q hq ↦ mem_iUnion.mpr ⟨⟨q, hq⟩, hqU ⟨q, hq⟩⟩
  obtain ⟨t, hKt⟩ := hK.elim_finite_subcover U hU this
  use (⋃ i ∈ t, U i), (⋂ i ∈ t, V i), ?_, ?_, hKt, ?_, ?_
  · exact isOpen_biUnion fun q' _ ↦ hU q'
  · exact isOpen_biInter_finset fun q' _ ↦ hV q'
  · exact mem_iInter₂_of_mem fun q' _ ↦ hpV q'
  refine disjoint_iUnion₂_left.mpr fun q' hq' ↦ ?_
  refine disjoint_of_subset_right ?_ (hUV q')
  exact biInter_subset_of_mem hq'

-- same theorem in Mathlib, though they haven't settled on doing it
-- primarily with set theory or with neighborhoods and filters.  this
-- one seems to be set-based
example {p : X} (hK : IsCompact K) (hpK : p ∉ K)
    : ∃ U W, IsOpen U ∧ IsOpen W ∧ K ⊆ U ∧ p ∈ W ∧ Disjoint U W :=
  IsCompact.separation_of_notMem hK hpK

-- feels like we could use an aux result, that a ⊆ ⋃ bᶜ ↔ a ⋂ b = ∅
-- but then also a ⋂ (⋂ b ∈ t) = ⋂ b ∈ insert a t
lemma t_2_6 {ι : Type u} [DecidableEq ι]
            (K : ι → Set X) (hKC : ∀ i, IsCompact (K i)) (hKI : (⋂ i, K i) = ∅)
    : ∃ (t : Finset ι), (⋂ i ∈ t, K i) = ∅ := by
  by_cases! h : IsEmpty ι
  · simpa using hKI
  have α := Classical.choice h
  -- TODO: c.f. biInter_insert?
  let S := {i : ι // i ≠ α}
  have hOpen (s : S) : IsOpen (K s)ᶜ := (hKC s).isClosed.isOpen_compl
  have : K α ⊆ ⋃ s : S, (K s)ᶜ := fun x hx ↦ by
    -- make the goal play nicer with contrapose
    rw [← compl_iInter, mem_compl_iff]
    contrapose! hKI
    use x, mem_iInter_of_mem fun i ↦ ?_
    by_cases! hi : i = α
    · exact hi ▸ hx
    · exact mem_iInter.mp hKI ⟨i, hi⟩
  obtain ⟨t, hKt⟩ := (hKC α).elim_finite_subcover _ hOpen this
  let t' := insert α (t.image Subtype.val)
  use t', iInter₂_eq_empty_iff.mpr fun x ↦ ?_
  by_cases! hx : x ∉ K α
  · use α, Finset.mem_insert_self _ _, hx
  · obtain ⟨i, hi, hx'⟩ := mem_iUnion₂.mp <| mem_of_mem_of_subset hx hKt
    use i, ?_
    · exact (mem_compl_iff (K i) x).mp hx'
    · exact Finset.mem_insert_of_mem <| Finset.mem_image_of_mem Subtype.val hi

-- this isn't quite the same, but it's morally the same I think.  one
-- big difference, that's really nice, is that they just start the proof
-- with "here's a distinguished compact set, and here's the index for
-- the rest".  that solves so many headaches up above.
example {ι : Type u} {K : Set X} (hK : IsCompact K) (k : ι → Set X)
        (hkc : ∀ i, IsClosed (k i)) (hKk : K ∩ (⋂ i, k i) = ∅)
    : ∃ (t : Finset ι), K ∩ (⋂ i ∈ t, k i) = ∅
    := IsCompact.elim_finite_subfamily_closed hK k hkc hKk

-- it's a little wild to me that Rudin just glosses this one.
-- You'd think it would at least get called out as an earlier lemma or
-- something?  But nope, one sentence handwave.  Like yeah, it is
-- obvious in English.  Just, damn, look how much work in Lean.
lemma t_2_7_aux [WeaklyLocallyCompactSpace X] (hK : IsCompact K)
    : ∃ U, IsOpen U ∧ IsCompact (closure U) ∧ K ⊆ U := by
  choose V hV hqV using fun (q : K) ↦ exists_compact_mem_nhds q.val
  choose V' hV'V hOpenV' hqV' using fun (q : K) ↦ mem_nhds_iff.mp <| hqV q
  have : K ⊆ ⋃ i, V' i := fun q hq ↦ mem_iUnion.mpr ⟨⟨q, hq⟩, hqV' ⟨q, hq⟩⟩
  obtain ⟨t, hKt⟩ := hK.elim_finite_subcover V' hOpenV' this
  -- change the signatures to better suit the biUnion lemmas
  have hOpenV' := fun q (_ : q ∈ t) ↦ hOpenV' q
  have hV := fun q (_ : q ∈ t) ↦ hV q
  have hV'V := fun q (_ : q ∈ t) ↦ hV'V q
  use (⋃ q ∈ t, V' q), isOpen_biUnion hOpenV', ?_, hKt
  refine exists_isCompact_superset_iff.mp ⟨?_, ?_, ?_⟩
  · exact ⋃ q ∈ t, V q
  · exact t.isCompact_biUnion hV
  · exact iUnion₂_mono hV'V

lemma t_2_7 [WeaklyLocallyCompactSpace X] {U : Set X}
            (hU : IsOpen U) (hK : IsCompact K) (hsub : K ⊆ U)
    : ∃ V, IsOpen V ∧ IsCompact (closure V) ∧ K ⊆ V ∧ closure V ⊆ U
    := by
  obtain ⟨G, ⟨IsOpenG, IsCompactClosureG, K_sub_G⟩⟩ := t_2_7_aux hK
  by_cases! hUniv : U = Set.univ
  · use G, IsOpenG, IsCompactClosureG, K_sub_G, hUniv ▸ subset_univ _
  let G' := Uᶜ ∩ (closure G)
  let C := {x // x ∈ Uᶜ}
  have hG' : IsCompact G' := IsCompact.inter_left IsCompactClosureG hU.isClosed_compl
  choose W V hW hV hKW hqV hWV using fun (q : C) ↦
    t_2_5 hK <| mem_of_mem_of_subset q.property <| compl_subset_compl_of_subset hsub
  let W' (c : C) := closure (W c)
  have hW' (c : C) := @isClosed_closure _ _ (W c)
  have hG'W' : G' ∩ (⋂ c, W' c) = ∅ := by
    refine eq_empty_iff_forall_notMem.mpr fun x ⟨⟨hxU, _⟩, hxW⟩ ↦ ?_
    exact Set.disjoint_left.mp ((hWV ⟨x, hxU⟩).closure_left (hV ⟨x, hxU⟩))
                               (mem_iInter.mp hxW ⟨x, hxU⟩) (hqV ⟨x, hxU⟩)
  obtain ⟨cs, hG'cs⟩ := hG'.elim_finite_subfamily_closed W' hW' hG'W'
  -- change the signatures to better suit the biUnion lemmas
  have hW := fun c (_ : c ∈ cs) ↦ hW c
  have hW' := fun c (_ : c ∈ cs) ↦ hW' c
  have hKW := fun c (_ : c ∈ cs) ↦ hKW c
  -- the one "big set" that sandwiches everything: it is closed, it contains
  -- `G ∩ ⋂ W`, and (by `hG'cs`) it misses `Uᶜ`
  have hBig : G ∩ (⋂ c ∈ cs, W c) ⊆ (closure G) ∩ (⋂ c ∈ cs, W' c) :=
  inter_subset_inter subset_closure <| iInter₂_mono fun _ _ ↦ subset_closure
  have hBigClosed : IsClosed ((closure G) ∩ (⋂ c ∈ cs, W' c)) :=
  isClosed_closure.inter <| isClosed_biInter hW'
  use G ∩ (⋂ c ∈ cs, W c), ?_, ?_, ?_, ?_
  · exact IsOpen.inter IsOpenG <| isOpen_biInter_finset hW
  · exact exists_isCompact_superset_iff.mp
          ⟨_, IsCompact.inter_right IsCompactClosureG <| isClosed_biInter hW', hBig⟩
  · exact subset_inter_iff.mpr ⟨K_sub_G, subset_iInter₂ hKW⟩
  · intro x hx
    obtain ⟨hxG, hxW⟩ := closure_minimal hBig hBigClosed hx
    by_contra hxU
    exact eq_empty_iff_forall_notMem.mp hG'cs x ⟨⟨hxU, hxG⟩, hxW⟩

end Hausdorff
