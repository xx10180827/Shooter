#include "ShooterRoundCommandlet.h"
#include "ShooterRoundTools.h"
DEFINE_LOG_CATEGORY_STATIC(LogShooterRound, Log, All);
UShooterRoundCommandlet::UShooterRoundCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}
int32 UShooterRoundCommandlet::Main(const FString& Params)
{
    FString Result;
    const bool bVerify = FParse::Param(*Params, TEXT("Verify"));
    const bool bOK = bVerify ? ShooterRoundTools::Verify(Result) : ShooterRoundTools::Migrate(Result);
    if (!bOK) { UE_LOG(LogShooterRound, Error, TEXT("%s"), *Result); return 1; }
    UE_LOG(LogShooterRound, Display, TEXT("%s"), *Result);
    return 0;
}
