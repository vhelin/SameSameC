CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_scan_orchestration_test
	$(BT) -s main.ssc
	./core_scan_orchestration_test > core_scan_orchestration_test.log 2>&1
	grep -q "primary_candidate_decision function=allocatorRaCoreScanOrchestrationA.*plan=1 status=complete" compiler.log
	grep -q "primary_candidate_decision function=allocatorRaCoreScanOrchestrationHl.*plan=4 status=complete" compiler.log
	grep -q "primary_candidate_decision function=allocatorRaCoreScanOrchestrationBoundary.*path_safe=no.*plan=3 status=complete" compiler.log
	grep -q "slot_transition_plan function=allocatorRaCoreScanOrchestrationHl.*decision=2.*clear=yes spill=yes retain=yes assign=yes status=complete" compiler.log
	grep -q "slot_transition_apply function=allocatorRaCoreScanOrchestrationHl.*decision=2.*order=clear_spill_retain_assign status=complete" compiler.log
	grep -q "candidate_transition function=allocatorRaCoreScanOrchestrationA.*decision=1.*retain=yes assign=yes status=complete" compiler.log
	grep -q "candidate_transition function=allocatorRaCoreScanOrchestrationHl.*decision=2.*clear=yes spill=yes retain=yes assign=yes status=complete" compiler.log
	grep -q "candidate_transition function=allocatorRaCoreScanOrchestrationBoundary.*decision=1.*status=complete" compiler.log
	grep -q "active_replaceability function=allocatorRaCoreScanOrchestrationA.*queried=no replaceable=yes reason=empty_slot status=complete" compiler.log
	grep -q "active_replaceability function=allocatorRaCoreScanOrchestrationHl.*queried=yes replaceable=yes reason=metadata_available status=complete" compiler.log
	test `grep -c "primary_candidate_decision function=coreDecision.*status=complete" core_scan_orchestration_test.log` -eq 6
	grep -q "primary_candidate_decision function=coreDecision.*allowed=no path_safe=no.*plan=2 status=complete" core_scan_orchestration_test.log
	grep -q "primary_candidate_decision function=coreDecision.*allowed=yes path_safe=no.*plan=6 status=complete" core_scan_orchestration_test.log
	grep -q "primary_candidate_decision function=badAllowed.*status=dependency_failed" core_scan_orchestration_test.log
	grep -q "primary_candidate_decision function=badPath.*status=dependency_failed" core_scan_orchestration_test.log
	grep -q "primary_candidate_decision function=badActive.*status=dependency_failed" core_scan_orchestration_test.log
	test `grep -c "primary_candidate_decision function=.*status=invalid_input" core_scan_orchestration_test.log` -eq 8
	grep -q "slot_transition_plan function=coreAssign.*spill=no retain=yes assign=yes status=complete" core_scan_orchestration_test.log
	grep -q "slot_transition_plan function=coreKeep.*retain=no assign=no status=complete" core_scan_orchestration_test.log
	grep -q "slot_transition_plan function=coreReplace.*displaced=r4 clear=yes spill=yes retain=yes assign=yes status=complete" core_scan_orchestration_test.log
	grep -q "slot_transition_apply function=coreReplace.*order=clear_spill_retain_assign status=complete" core_scan_orchestration_test.log
	grep -q "slot_transition_apply function=doubleApply.*status=invalid_input" core_scan_orchestration_test.log
	grep -q "slot_transition_plan function=coreReplaceNoClear.*clear=no spill=yes.*status=complete" core_scan_orchestration_test.log
	test `grep -c "slot_transition_apply function=bad.*status=callback_failed" core_scan_orchestration_test.log` -eq 3
	grep -q "slot_transition_apply function=badRetainValue.*callback=retain_candidate status=invalid_callback" core_scan_orchestration_test.log
	grep -q "slot_transition_apply function=staleTransition.*status=invalid_input" core_scan_orchestration_test.log
	grep -q "slot_transition_plan function=badSlotState.*status=invalid_input" core_scan_orchestration_test.log
	test `grep -c "slot_transition_plan function=.*status=invalid_input" core_scan_orchestration_test.log` -eq 7
	grep -q "candidate_transition function=coreCandidateTransitionAssign.*prefer=no decision=1.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition function=coreCandidateTransitionNearer.*prefer=no decision=2 clear=yes spill=yes retain=yes assign=yes status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition function=coreCandidateTransitionEqualPreferred.*prefer=yes decision=2.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition function=coreCandidateTransitionEqualKept.*prefer=no decision=3.*retain=no assign=no status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition function=coreCandidateTransitionUnavailable.*replaceable=no.*decision=3.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition function=coreCandidateTransitionBadPreference.*status=dependency_failed" core_scan_orchestration_test.log
	grep -q "candidate_transition function=coreCandidateTransitionBadMetadata.*status=dependency_failed" core_scan_orchestration_test.log
	grep -q "candidate_transition function=coreCandidateTransitionBadSlot.*status=dependency_failed" core_scan_orchestration_test.log
	test `grep -c "candidate_transition function=.*status=invalid_input" core_scan_orchestration_test.log` -eq 14
	grep -q "candidate_transition_prepare function=allocatorRaCoreScanOrchestrationA.*order=slot_resolve_transition_plan status=complete" compiler.log
	grep -q "candidate_transition_prepare function=allocatorRaCoreScanOrchestrationHl.*order=slot_resolve_transition_plan status=complete" compiler.log
	grep -q "candidate_transition_prepare function=coreTransitionPreparePrimary.*slot=0 name=primary decision=1 order=slot_resolve_transition_plan status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition_prepare function=coreTransitionPrepareAlternate.*slot=1 name=alternate decision=1 order=slot_resolve_transition_plan status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition_prepare function=coreTransitionPrepareBadCount.*status=invalid_policy" core_scan_orchestration_test.log
	grep -q "candidate_transition_prepare function=coreTransitionPrepareCountMismatch.*status=invalid_policy" core_scan_orchestration_test.log
	grep -q "candidate_transition_prepare function=coreTransitionPrepareBadIndex.*status=invalid_policy" core_scan_orchestration_test.log
	grep -q "candidate_transition_prepare function=coreTransitionPrepareNullName.*status=invalid_policy" core_scan_orchestration_test.log
	grep -q "candidate_transition_prepare function=coreTransitionPrepareEmptyName.*status=invalid_policy" core_scan_orchestration_test.log
	grep -q "candidate_transition_prepare function=coreTransitionPrepareBadMetadata.*stage=transition_plan status=dependency_failed" core_scan_orchestration_test.log
	test `grep -c "candidate_transition_prepare function=coreTransitionPrepare.*status=invalid_input" core_scan_orchestration_test.log` -eq 2
	grep -q "candidate_transition_execute function=allocatorRaCoreScanOrchestrationA.*order=prepare_apply status=complete" compiler.log
	grep -q "candidate_transition_execute function=allocatorRaCoreScanOrchestrationHl.*order=prepare_apply status=complete" compiler.log
	grep -q "candidate_transition_execute function=coreTransitionExecuteAssign.*decision=1 retained=yes spill=no order=prepare_apply status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition_execute function=coreTransitionExecutePreserve.*decision=1 retained=yes spill=yes order=prepare_apply status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition_execute function=coreTransitionExecuteReplace.*decision=2 retained=yes spill=no order=prepare_apply status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition_execute function=coreTransitionExecuteKeep.*active=r4 active_next=20.*decision=3 retained=no spill=no order=prepare_apply status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition_execute function=coreTransitionExecuteBadPolicy.*stage=preparation status=dependency_failed" core_scan_orchestration_test.log
	grep -q "candidate_transition_execute function=coreTransitionExecuteBadRetain.*stage=application status=dependency_failed" core_scan_orchestration_test.log
	grep -q "candidate_transition_execute function=coreTransitionExecuteBadSpill.*stage=application status=dependency_failed" core_scan_orchestration_test.log
	test `grep -c "candidate_transition_execute function=coreTransitionExecute.*status=invalid_input" core_scan_orchestration_test.log` -eq 2
	grep -q "active_replaceability function=coreActiveReplaceabilityEmpty.*queried=no replaceable=yes reason=empty_slot status=complete" core_scan_orchestration_test.log
	grep -q "active_replaceability function=coreActiveReplaceabilityExpired.*queried=no replaceable=yes reason=active_expired status=complete" core_scan_orchestration_test.log
	grep -q "active_replaceability function=coreActiveReplaceabilityAvailable.*queried=yes replaceable=yes reason=metadata_available status=complete" core_scan_orchestration_test.log
	grep -q "active_replaceability function=coreActiveReplaceabilityMissing.*queried=yes replaceable=no reason=metadata_missing status=complete" core_scan_orchestration_test.log
	grep -q "active_replaceability function=coreActiveReplaceabilityBadCallback.*queried=yes replaceable=no status=invalid_callback" core_scan_orchestration_test.log
	test `grep -c "active_replaceability function=coreActiveReplaceability.*status=invalid_input" core_scan_orchestration_test.log` -eq 7
	grep -q "active_replaceability function=<null>.*status=invalid_input" core_scan_orchestration_test.log
	grep -q "candidate_selection_finalize function=allocatorRaCoreScanOrchestrationA.*proceed=yes.*order=decide_dispatch_split status=complete" compiler.log
	grep -q "candidate_selection_finalize function=allocatorRaCoreScanOrchestrationHl.*proceed=yes.*order=decide_dispatch_split status=complete" compiler.log
	grep -q "candidate_selection_finalize function=coreFinalizePrimary.*proceed=yes selected=10 split=1.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_finalize function=coreFinalizeAlternate.*plan=2 dispatch=2 proceed=yes selected=20 split=1.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_finalize function=coreFinalizeRejectUnsupported.*proceed=no.*reason=unsupported_physical_register status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_finalize function=coreFinalizeRejectPath.*proceed=no.*reason=clobber_between status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_finalize function=coreFinalizeConflictPrimary.*proceed=yes selected=10 split=2.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_finalize function=coreFinalizeSplitUnsupported.*proceed=no.*split=3 reason=multi_read_interval status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_finalize function=coreFinalizeSplitRegister.*proceed=no.*split=4 reason=multi_read_preserve_requires_register status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_finalize function=coreFinalizeAlternatePreserve.*proceed=yes selected=20 split=2.*status=complete" core_scan_orchestration_test.log
	test `grep -c "candidate_selection_finalize function=coreFinalize.*status=dependency_failed" core_scan_orchestration_test.log` -eq 3
	test `grep -c "candidate_selection_finalize function=coreFinalize.*status=invalid_input" core_scan_orchestration_test.log` -eq 2
	grep -q "candidate_selection_prepare function=allocatorRaCoreScanOrchestrationA.*single_read=yes.*preservation_queried=no.*reason=single_read status=complete" compiler.log
	grep -q "candidate_selection_prepare function=allocatorRaCoreScanOrchestrationHl.*proceed=yes.*status=complete" compiler.log
	grep -q "candidate_selection_prepare function=corePrepareSingle.*proceed=yes.*preservation_queried=no.*reason=single_read status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_prepare function=corePrepareSupported.*preservation_queried=yes preservation_supported=yes preservation_phy=10.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_prepare function=corePrepareUnsupported.*preservation_queried=yes preservation_supported=no preservation_phy=-1.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_prepare function=corePrepareBoundaryBetween.*proceed=no blocked=yes preservation_queried=no.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_prepare function=corePrepareBoundaryOutside.*proceed=yes blocked=no preservation_queried=yes preservation_supported=yes.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_prepare function=corePrepareUnconditional.*proceed=no blocked=yes preservation_queried=no.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_prepare function=corePrepareMissingBoundary.*proceed=no blocked=yes preservation_queried=no.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_prepare function=corePrepareBadCallback.*callback=can_preserve_split_spill status=invalid_callback" core_scan_orchestration_test.log
	grep -q "candidate_selection_prepare function=corePrepareNoPolicy.*preservation_queried=no preservation_supported=no preservation_phy=-1 reason=policy_unavailable status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_prepare function=corePrepareBadReasons.*stage=interval_blocking status=dependency_failed" core_scan_orchestration_test.log
	test `grep -c "candidate_selection_prepare function=corePrepare.*status=invalid_input" core_scan_orchestration_test.log` -eq 2
	grep -q "candidate_selection_orchestrate function=allocatorRaCoreScanOrchestrationA.*proceed=yes.*order=prepare_finalize status=complete" compiler.log
	grep -q "candidate_selection_orchestrate function=allocatorRaCoreScanOrchestrationHl.*proceed=yes.*order=prepare_finalize status=complete" compiler.log
	grep -q "candidate_selection_orchestrate function=coreOrchestratePrimary.*proceed=yes selected=10 split=1 order=prepare_finalize status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_orchestrate function=coreOrchestrateBlocked.*proceed=no stage=preparation reason=spill_boundary status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_orchestrate function=coreOrchestratePreserve.*proceed=yes selected=10 split=2 order=prepare_finalize status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_orchestrate function=coreOrchestrateUnsupported.*proceed=no selected=10 split=3 order=prepare_finalize status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_orchestrate function=coreOrchestrateAlternate.*proceed=no selected=20 split=4 order=prepare_finalize status=complete" core_scan_orchestration_test.log
	grep -q "candidate_selection_orchestrate function=coreOrchestrateBadPreserve.*stage=preparation status=dependency_failed" core_scan_orchestration_test.log
	grep -q "candidate_selection_orchestrate function=coreOrchestrateBadFact.*stage=finalization status=dependency_failed" core_scan_orchestration_test.log
	grep -q "candidate_selection_orchestrate function=coreOrchestrateBadReasons.*stage=preparation status=dependency_failed" core_scan_orchestration_test.log
	test `grep -c "candidate_selection_orchestrate function=coreOrchestrate.*status=invalid_input" core_scan_orchestration_test.log` -eq 1
	grep -q "candidate_transition_apply function=allocatorRaCoreScanOrchestrationA.*order=slot_prepare_spill status=complete" compiler.log
	grep -q "candidate_transition_apply function=allocatorRaCoreScanOrchestrationHl.*order=slot_prepare_spill status=complete" compiler.log
	grep -q "candidate_transition_apply function=coreCandidateApplyAssign.*split_action=none retained=yes spill=no.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition_apply function=coreCandidateApplyKeep.*split_action=preserve retained=no spill=no.*status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition_apply function=coreCandidateApplyPreserve.*split_action=preserve retained=yes spill=yes.*order=slot_prepare_spill status=complete" core_scan_orchestration_test.log
	grep -q "candidate_transition_apply function=coreCandidateApplyBadSpill.*stage=spill_apply status=dependency_failed" core_scan_orchestration_test.log
	grep -q "candidate_transition_apply function=coreCandidateApplyBadRetain.*stage=slot_transition status=dependency_failed" core_scan_orchestration_test.log
	grep -q "candidate_transition_apply function=coreCandidateApplyStale.*stage=slot_transition status=dependency_failed" core_scan_orchestration_test.log
	test `grep -c "candidate_transition_apply function=.*status=invalid_input" core_scan_orchestration_test.log` -eq 2
	! grep -q "candidate_transition function=allocatorRaCoreScanOrchestration.*status=invalid" compiler.log
	! grep -q "candidate_transition_prepare function=allocatorRaCoreScanOrchestration.*status=invalid\|candidate_transition_prepare function=allocatorRaCoreScanOrchestration.*status=dependency_failed" compiler.log
	! grep -q "candidate_transition_execute function=allocatorRaCoreScanOrchestration.*status=invalid\|candidate_transition_execute function=allocatorRaCoreScanOrchestration.*status=dependency_failed" compiler.log
	! grep -q "active_replaceability function=allocatorRaCoreScanOrchestration.*status=invalid" compiler.log
	! grep -q "candidate_selection_finalize function=allocatorRaCoreScanOrchestration.*status=invalid\|candidate_selection_finalize function=allocatorRaCoreScanOrchestration.*status=dependency_failed" compiler.log
	! grep -q "candidate_selection_prepare function=allocatorRaCoreScanOrchestration.*status=invalid\|candidate_selection_prepare function=allocatorRaCoreScanOrchestration.*status=dependency_failed" compiler.log
	! grep -q "candidate_selection_orchestrate function=allocatorRaCoreScanOrchestration.*status=invalid\|candidate_selection_orchestrate function=allocatorRaCoreScanOrchestration.*status=dependency_failed" compiler.log
	! grep -q "candidate_transition_apply function=allocatorRaCoreScanOrchestration.*status=invalid\|candidate_transition_apply function=allocatorRaCoreScanOrchestration.*status=dependency_failed" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_scan_orchestration_test: core_scan_orchestration_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_scan_orchestration_test core_scan_orchestration_test.c ../../../../compiler/register_allocator.c


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_scan_orchestration_test core_scan_orchestration_test.log
