#include <Windows.h>
#include <Evntprov.h>
#include <wmistr.h>
#include <evntrace.h>
#include <evntprov.h>

typedef struct _HELLO_WORLD_TRACE_CONTEXT
{
	TRACEHANDLE            RegistrationHandle;
	TRACEHANDLE            Logger;      // Used as pointer to provider traits.
	ULONGLONG              MatchAnyKeyword;
	ULONGLONG              MatchAllKeyword;
	ULONG                  Flags;
	ULONG                  IsEnabled;
	UCHAR                  Level;
	UCHAR                  Reserve;
	USHORT                 EnableBitsCount;
	PULONG                 EnableBitMask;
	const ULONGLONG* EnableKeyWords;
	const UCHAR* EnableLevel;
} HELLO_WORLD_TRACE_CONTEXT, *PHELLO_WORLD_TRACE_CONTEXT;

#define MCGEN_EVENT_BIT_SET(EnableBits, BitPosition) \
	((((const unsigned char*)EnableBits)[BitPosition >> 3] & (1u << (BitPosition & 7))) != 0)

DECLSPEC_CACHEALIGN ULONG Hello_World_Metric_ProviderEnableBits[1];

// Maps for easy access
const ULONGLONG Hello_World_Metric_ProviderKeywords[1] = { 0x0 };
const unsigned char Hello_World_Metric_ProviderLevels[1] = { 0 };

const EVENT_DESCRIPTOR SIMPLE_METRIC = 
{
	.Id = 0x1,
	.Version= 0x1,
	.Channel = 0x0,
	.Level = 0x0,
	.Opcode = 0x0,
	.Task = 0x0,
	.Keyword = 0x0
};

// 06A2EA53-FC6C-42E5-9176-18749AB2CA13
const GUID HELLO_METRIC = { 0x06a2ea53, 0xfc6c, 0x42e5, {0x91, 0x76, 0x18, 0x74, 0x9a, 0xb2, 0xca, 0x13} };
// In the binary: 53h, 0EAh, 0A2h, 6, 6Ch, 0FCh, 0E5h, 42h, 91h, 76h, 18h, 74h, 9Ah, 0B2h, 0CAh, 13h, 10h
// The layout of the guid in the binary is: {04 03 02 01 - 06 05 - 08 07 -09 10 - 11 12 13 14 15 16}

HELLO_WORLD_TRACE_CONTEXT HELLO_METRIC_Context =
{ 
	.RegistrationHandle = 0, 
	.Logger = 0, 
	.MatchAnyKeyword = 0,
	.MatchAllKeyword = 0,
	.Flags = 0,
	.IsEnabled = 0,
	.Level = 0,
	.Reserve = 0,
	.EnableBitsCount = 1,
	.EnableBitMask = Hello_World_Metric_ProviderEnableBits,
	.EnableKeyWords = Hello_World_Metric_ProviderKeywords, 
	.EnableLevel = Hello_World_Metric_ProviderLevels 
};

VOID
__stdcall
ProviderControlCallback(
	_In_ LPCGUID SourceId,
	_In_ ULONG ControlCode,
	_In_ UCHAR Level,
	_In_ ULONGLONG MatchAnyKeyword,
	_In_ ULONGLONG MatchAllKeyword,
	_In_opt_ PEVENT_FILTER_DESCRIPTOR FilterData,
	_Inout_opt_ PVOID CallbackContext
);

ULONG
WriteSimpleMetricEvent(
	_In_ PHELLO_WORLD_TRACE_CONTEXT Context,
	_In_ PCEVENT_DESCRIPTOR Descriptor,
	_In_opt_ PCWSTR  MetricName,
	_In_ const signed int  MetricValue
);


int main()
{
	// register this program with ETW
	EventRegister(
		&HELLO_METRIC, 
		ProviderControlCallback, 
		&HELLO_METRIC_Context, 
		&HELLO_METRIC_Context.RegistrationHandle
		);

		for (int i = 0; i < 10; ++i) {
			if (MCGEN_EVENT_BIT_SET(Hello_World_Metric_ProviderEnableBits, 0)) {
				WriteSimpleMetricEvent(&HELLO_METRIC_Context, &SIMPLE_METRIC, L"test event", i);
			}
		}

	// unregister this program with ETW
	EventUnregister(HELLO_METRIC_Context.RegistrationHandle);
}

FORCEINLINE
BOOLEAN
McGenLevelKeywordEnabled(
	_In_ PHELLO_WORLD_TRACE_CONTEXT EnableInfo,
	_In_ UCHAR Level,
	_In_ ULONGLONG Keyword
)
{
	if ((Level <= EnableInfo->Level) || // This also covers the case of Level == 0.
		(EnableInfo->Level == 0)) {

		if ((Keyword == (ULONGLONG)0) ||
			((Keyword & EnableInfo->MatchAnyKeyword) &&
			((Keyword & EnableInfo->MatchAllKeyword) == EnableInfo->MatchAllKeyword))) {
			return TRUE;
		}
	}

	return FALSE;
}

VOID
__stdcall
ProviderControlCallback(
	_In_ LPCGUID SourceId,
	_In_ ULONG ControlCode,
	_In_ UCHAR Level,
	_In_ ULONGLONG MatchAnyKeyword,
	_In_ ULONGLONG MatchAllKeyword,
	_In_opt_ PEVENT_FILTER_DESCRIPTOR FilterData,
	_Inout_opt_ PVOID CallbackContext
)
{
	PHELLO_WORLD_TRACE_CONTEXT Ctx = (PHELLO_WORLD_TRACE_CONTEXT)CallbackContext;
	ULONG Ix;

	UNREFERENCED_PARAMETER(SourceId);
	UNREFERENCED_PARAMETER(FilterData);

	if (Ctx == NULL) {
		return;
	}

	switch (ControlCode) {

	case EVENT_CONTROL_CODE_ENABLE_PROVIDER:
		Ctx->Level = Level;
		Ctx->MatchAnyKeyword = MatchAnyKeyword;
		Ctx->MatchAllKeyword = MatchAllKeyword;
		Ctx->IsEnabled = EVENT_CONTROL_CODE_ENABLE_PROVIDER;

		//
		// For each event, mark if it's enabled according to MatchAnyKeyword and MatchAllKeyword
		//
		for (Ix = 0; Ix < Ctx->EnableBitsCount; Ix += 1) {
			if (McGenLevelKeywordEnabled(Ctx, Ctx->EnableLevel[Ix], Ctx->EnableKeyWords[Ix]) != FALSE) {
				Ctx->EnableBitMask[Ix >> 5] |= (1 << (Ix % 32));
			}
			else {
				Ctx->EnableBitMask[Ix >> 5] &= ~(1 << (Ix % 32));
			}
		}
		break;

	case EVENT_CONTROL_CODE_DISABLE_PROVIDER:
		Ctx->IsEnabled = EVENT_CONTROL_CODE_DISABLE_PROVIDER;
		Ctx->Level = 0;
		Ctx->MatchAnyKeyword = 0;
		Ctx->MatchAllKeyword = 0;

		if (Ctx->EnableBitsCount > 0) {
			RtlZeroMemory(Ctx->EnableBitMask, (((Ctx->EnableBitsCount - 1) / 32) + 1) * sizeof(ULONG));
		}
		break;

	default:
		break;
	}

	return;
}


ULONG
WriteSimpleMetricEvent(
	_In_ PHELLO_WORLD_TRACE_CONTEXT Context,
	_In_ PCEVENT_DESCRIPTOR Descriptor,
	_In_opt_ PCWSTR  MetricName,
	_In_ const signed int  MetricValue
)
{
	EVENT_DATA_DESCRIPTOR EventData[3];

	RtlZeroMemory(EventData, sizeof(EventData));

	if (MetricName != NULL) {
		EventData[1].Ptr = (ULONGLONG)(ULONG_PTR)MetricName;
		EventData[1].Size = (ULONG)((wcslen(MetricName) + 1) * sizeof(WCHAR));
	} else {
		EventData[1].Ptr = (ULONGLONG)(ULONG_PTR)L"NULL";
		EventData[1].Size = sizeof(L"NULL");
	}
	
	EventData[2].Ptr = (ULONGLONG)(ULONG_PTR)&MetricValue;
	EventData[2].Size = sizeof(const signed int);
	EventData[2].Reserved = 0;

	return EventWriteTransfer(Context->RegistrationHandle, Descriptor, NULL, NULL, 3, &EventData[0]);
}