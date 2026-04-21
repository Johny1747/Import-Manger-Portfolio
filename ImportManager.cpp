// Fill out your copyright notice in the Description page of Project Settings.


#include "ImportManager.h"
#include "JsonHelpers.h"
#include "IImageWrapper.h"
#include "FileLibraryManager.h"
#include "IImageWrapperModule.h"

void UImportManager::InitializeImportManager()
{
	// Get File Library Manager reference
	FileLibraryManager = GetGameInstance()->GetSubsystem<UFileLibraryManager>();
	// Load Register
	LoadRegister();

}

// For users to have the ability to use imported images such as floor plans or logos
// we want to have a system, that will allow imports and store the image files with the required meta which is crucial
// for future features such as exporting the project with all the required files and transfering the project to other users etc.

#pragma region Image Meta Functions

FString UImportManager::GetSaveMetaJson(const FFileInfo& FileInfo, TArray<uint8>& Data)
{
	FImgFileMeta ImgMetaToSave;
	ImgMetaToSave.ImageName = FileInfo.FileName;
	ImgMetaToSave.RawFileData = Data;
	AddCodeToImgMeta(ImgMetaToSave);
	FString JsonString = ImgMetaToJson(ImgMetaToSave);
    return JsonString;
}

FImgFileMeta UImportManager::MakeSaveMeta(const FFileInfo& FileInfo,const TArray<uint8>& Data)
{
    FImgFileMeta ImgMetaToSave;
    ImgMetaToSave.ImageName = FileInfo.FileName;
    ImgMetaToSave.RawFileData = Data;
    AddCodeToImgMeta(ImgMetaToSave);
    return ImgMetaToSave; 
}

FString UImportManager::GetJsonFromMeta(const FImgFileMeta& Meta)
{    
    return ImgMetaToJson(Meta);
}

void UImportManager::AddCodeToImgMeta(FImgFileMeta& ImgMetaRef)
{
    // Create Code
	FString FileHash = FMD5::HashBytes(ImgMetaRef.RawFileData.GetData(), ImgMetaRef.RawFileData.Num());
	FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d%H%M%S%s"));
	ImgMetaRef.Code = FMD5::HashAnsiString(*(FileHash + Timestamp));
}

FString UImportManager::ImgMetaToJson(const FImgFileMeta& ImgMetaIn)
{
	
    // 1. Create a new JSON Object
    TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();

    // 2. Use your JsonUtil helpers to set the standard string fields
    JsonUtil::SetString(JsonObj, TEXT("ImageName"), ImgMetaIn.ImageName);
    JsonUtil::SetString(JsonObj, TEXT("Code"), ImgMetaIn.Code);

    // Note: We are explicitly skipping the UTexture2D* LoadedTexture per your instructions.

    // 3. Handle the Raw Binary Data
    // We must encode the uint8 array into a Base64 string so it survives JSON serialization.
    if (ImgMetaIn.RawFileData.Num() > 0)
    {
        FString Base64Data = FBase64::Encode(ImgMetaIn.RawFileData);
        JsonUtil::SetString(JsonObj, TEXT("RawFileData"), Base64Data);
    }
    else
    {
        // If it's empty, we can just write an empty string
        JsonUtil::SetString(JsonObj, TEXT("RawFileData"), TEXT(""));
    }

    // 4. Serialize the JSON Object down into a final FString
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);

    return OutputString;

}

FImgFileMeta UImportManager::JsonToImgMeta(const FString& JsonStringIn, bool UnpackFile)
{
    FImgFileMeta OutMeta;

    FString TrimmedString = JsonStringIn.TrimStartAndEnd();
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TrimmedString);

    // 1. Create a JSON Reader
    //TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStringIn);
    TSharedPtr<FJsonObject> JsonObj;

    // 2. Deserialize the text string into a structured JSON Object
    if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
    {
        // 3. Extract the standard properties using your JsonUtil helpers
        JsonUtil::TryGetString(JsonObj, TEXT("ImageName"), OutMeta.ImageName);
        JsonUtil::TryGetString(JsonObj, TEXT("Code"), OutMeta.Code);

        if (UnpackFile)
        {
            // 4. Extract and Decode the Base64 image data
            FString Base64Data;
            if (JsonUtil::TryGetString(JsonObj, TEXT("RawFileData"), Base64Data) && !Base64Data.IsEmpty())
            {
                // This safely translates the text string back into the TArray<uint8>
                FBase64::Decode(Base64Data, OutMeta.RawFileData);
            }

            OutMeta.LoadedTexture = LoadTextureFromRawData(OutMeta.RawFileData);
            OutMeta.RawFileData.Empty(); // Clear the raw data from memory since we now have the texture loaded
        }
        
    }
    else
    {
        // This will tell you the exact line and character where the parser gave up
        UE_LOG(LogTemp, Error, TEXT("JSON Parse Error: %s"), *Reader->GetErrorMessage());
    }


    return OutMeta;
}

#pragma endregion


#pragma region Register Management

bool UImportManager::LoadRegister()
{
    FString json;
    if (FileLibraryManager->ReadJsonFromFile(TEXT("Register.json"), E_SubFolder::Settings, json))
    {
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(json);
        TSharedPtr<FJsonObject> RootObj; // Changed to Object to match your save format

        if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* JsonArray;
            if (RootObj->TryGetArrayField(TEXT("ImportedImages"), JsonArray))
            {
                ImportAssetRegister.Empty(); // Clear before loading

                for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
                {
                    TSharedPtr<FJsonObject> Obj = Value->AsObject();
                    if (Obj.IsValid())
                    {
                        FImportAssetRegData Entry;
                        JsonUtil::TryGetString(Obj, TEXT("Code"), Entry.Code);
                        JsonUtil::TryGetString(Obj, TEXT("Name"), Entry.Name);
                        ImportAssetRegister.Add(Entry);
                    }
                }
                return true; // FIXED: Added missing return statement
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("LoadRegister: Failed to parse Register.json or it's empty."));
    }
    else
    {
        ResetRegister();
        return ImportAssetRegister.Num() > 0;
    }

    return false;
}

void UImportManager::ResetRegister()
{
    TArray<FFileInfo> ImportedAssets = FileLibraryManager->GetAllImageInfoInImageSubFolder();
    if (ImportedAssets.Num() > 0)
    {
        ImportAssetRegister.Empty();

        for (const FFileInfo& inf : ImportedAssets)
        {
            FImgFileMeta Meta = JsonToImgMeta(FileLibraryManager->LoadImgMetaJson(inf));
            FImportAssetRegData NewEntry;
            NewEntry.Code = Meta.Code;
            NewEntry.Name = Meta.ImageName;

            ImportAssetRegister.Add(NewEntry); // FIXED: Actually add it to the array
        }

        SaveRegisterToFile(); // Use the new DRY helper
    }
}

void UImportManager::AddRegisterEntry(const FString& Code, const FString& Name)
{
    // 1. Safety check to prevent duplicates
    for (const FImportAssetRegData& Entry : ImportAssetRegister)
    {
        if (Entry.Code == Code) return;
    }

    // 2. Add the new entry
    FImportAssetRegData NewEntry;
    NewEntry.Code = Code;
    NewEntry.Name = Name;
    ImportAssetRegister.Add(NewEntry);

    // 3. Save to disk
    SaveRegisterToFile();
}

void UImportManager::RemoveRegisterEntry(const FString& Code)
{
    // RemoveAll is a fast, built-in Unreal function that removes any element matching the lambda condition
    int32 RemovedCount = ImportAssetRegister.RemoveAll([&](const FImportAssetRegData& Entry) {
        return Entry.Code == Code;
    });

    // Only save to disk if we actually found and removed something
    if (RemovedCount > 0)
    {
        SaveRegisterToFile();
    }
}

FString UImportManager::GetCodeFromName(const FString& Name)
{
    for (const FImportAssetRegData& Entry : ImportAssetRegister)
    {
        if (Entry.Name == Name)
        {
            return Entry.Code;
        }
	}
    return FString();
}

FImgFileMeta UImportManager::GetMetaFromCode(const FString& Code, bool LoadTexture) 
{
    FString JsonString = FileLibraryManager->GetImgJsonByFileName(Code);
    return JsonToImgMeta(JsonString, LoadTexture);
}

#pragma endregion



#pragma region Utility Functions

UTexture2D* UImportManager::LoadTextureFromRawData(TArray<uint8>& RawData)
{
    // 1. Safety check
    if (RawData.Num() == 0)
    {
        return nullptr;
    }

    // 2. Load the Image Wrapper Module
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

    // 3. Auto-detect the image format (PNG, JPG, BMP, etc.) from the raw bytes
    EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(RawData.GetData(), RawData.Num());
    if (ImageFormat == EImageFormat::Invalid)
    {
        UE_LOG(LogTemp, Warning, TEXT("LoadTextureFromMeta: Unrecognized image format."));
        return nullptr;
    }

    // 4. Create the wrapper for that specific format
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
    if (!ImageWrapper.IsValid())
    {
        return nullptr;
    }

    // 5. Decompress the image
    if (ImageWrapper->SetCompressed(RawData.GetData(), RawData.Num()))
    {
        TArray<uint8> UncompressedBGRA;

        // Extract the raw pixels into a format Unreal expects (Blue, Green, Red, Alpha - 8 bit)
        if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA))
        {
            // 6. Create the empty Transient Texture in RAM
            UTexture2D* NewTexture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8);
            if (!NewTexture) return nullptr;

            // 7. Lock the texture's memory so we can write to it safely
            void* TextureData = NewTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);

            // 8. Copy our uncompressed pixel data directly into the texture's memory
            FMemory::Memcpy(TextureData, UncompressedBGRA.GetData(), UncompressedBGRA.Num());

            // 9. Unlock and send to the GPU
            NewTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
            NewTexture->UpdateResource();

            return NewTexture;
        }
    }

    return nullptr;
}

void UImportManager::SaveRegisterToFile()
{
    TSharedPtr<FJsonObject> RootObj = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> JsonEntries;

    for (const FImportAssetRegData& Entry : ImportAssetRegister)
    {
        TSharedPtr<FJsonObject> EntryObj = MakeShared<FJsonObject>();
        JsonUtil::SetString(EntryObj, TEXT("Code"), Entry.Code);
        JsonUtil::SetString(EntryObj, TEXT("Name"), Entry.Name);
        JsonEntries.Add(MakeShared<FJsonValueObject>(EntryObj));
    }
    RootObj->SetArrayField(TEXT("ImportedImages"), JsonEntries);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);

    FileLibraryManager->WriteJsonToFile(TEXT("Register.json"), E_SubFolder::Settings, OutputString);
}

#pragma endregion
