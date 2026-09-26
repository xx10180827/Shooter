#include "ShooterAIHUDCommandlet.h"
#include "ShooterAIHUDTools.h"
DEFINE_LOG_CATEGORY_STATIC(LogShooterAIHUD, Log, All);
UShooterAIHUDCommandlet::UShooterAIHUDCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}
int32 UShooterAIHUDCommandlet::Main(const FString& Params)
{
    FString Result;
    if (FParse::Param(*Params, TEXT("Migrate")))
    {
        if (!ShooterAIHUDTools::Migrate(Result)) { UE_LOG(LogShooterAIHUD, Error, TEXT("%s"), *Result); return 1; }
        UE_LOG(LogShooterAIHUD, Display, TEXT("%s"), *Result);
    }
    if (FParse::Param(*Params, TEXT("Verify")))
    {
        if (!ShooterAIHUDTools::Verify(Result)) { UE_LOG(LogShooterAIHUD, Error, TEXT("%s"), *Result); return 2; }
        UE_LOG(LogShooterAIHUD, Display, TEXT("%s"), *Result);
    }
    return 0;
}
