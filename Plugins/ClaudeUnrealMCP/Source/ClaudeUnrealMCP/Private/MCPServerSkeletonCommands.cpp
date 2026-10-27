// Skeleton socket authoring.
//
// USkeleton::Sockets is a UPROPERTY but is not reflected for Blueprint/Python
// access ("protected and cannot be read" from the Python console) - it can
// only be read/written from native C++. The correct engine API for adding a
// socket safely (proper transaction, notifies the skeleton editor if one is
// open, etc.) is IEditableSkeleton::AddSocket(), obtained via
// ISkeletonEditorModule::CreateEditableSkeleton() - this does NOT require an
// editor window to already be open for the asset.
#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMeshSocket.h"
#include "EditorAssetLibrary.h"
#include "ISkeletonEditorModule.h"
#include "IEditableSkeleton.h"
#include "Modules/ModuleManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"

FString FMCPServer::HandleAddSocket(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString SkeletonPath, BoneName, SocketName;
	if (!Params->TryGetStringField(TEXT("skeleton_path"), SkeletonPath))
		return MakeError(TEXT("skeleton_path required"));
	if (!Params->TryGetStringField(TEXT("bone_name"), BoneName))
		return MakeError(TEXT("bone_name required"));
	if (!Params->TryGetStringField(TEXT("socket_name"), SocketName))
		return MakeError(TEXT("socket_name required"));

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
		return MakeError(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	const FName SocketFName(*SocketName);
	for (USkeletalMeshSocket* Existing : Skeleton->Sockets)
	{
		if (Existing && Existing->SocketName == SocketFName)
		{
			return MakeError(FString::Printf(TEXT("Socket already exists: %s (use set_socket_transform to edit it)"), *SocketName));
		}
	}

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	TSharedRef<IEditableSkeleton> EditableSkeleton = SkeletonEditorModule.CreateEditableSkeleton(Skeleton);

	USkeletalMeshSocket* NewSocket = EditableSkeleton->AddSocket(FName(*BoneName));
	if (!NewSocket)
		return MakeError(FString::Printf(TEXT("Failed to add socket on bone: %s (check the bone name exists)"), *BoneName));

	// AddSocket() creates it with an auto-generated name (e.g. "<BoneName>Socket");
	// rename through the editable-skeleton API so the rename is tracked correctly.
	EditableSkeleton->RenameSocket(NewSocket->SocketName, SocketFName, nullptr);

	double X = 0.0, Y = 0.0, Z = 0.0, Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
	Params->TryGetNumberField(TEXT("x"), X);
	Params->TryGetNumberField(TEXT("y"), Y);
	Params->TryGetNumberField(TEXT("z"), Z);
	Params->TryGetNumberField(TEXT("pitch"), Pitch);
	Params->TryGetNumberField(TEXT("yaw"), Yaw);
	Params->TryGetNumberField(TEXT("roll"), Roll);

	NewSocket->RelativeLocation = FVector(X, Y, Z);
	NewSocket->RelativeRotation = FRotator(Pitch, Yaw, Roll);
	NewSocket->RelativeScale = FVector::OneVector;

	Skeleton->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(Skeleton->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("socket_name"), SocketName);
	Data->SetStringField(TEXT("bone_name"), BoneName);
	Data->SetStringField(TEXT("skeleton"), SkeletonPath);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetSocketTransform(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString SkeletonPath, SocketName;
	if (!Params->TryGetStringField(TEXT("skeleton_path"), SkeletonPath))
		return MakeError(TEXT("skeleton_path required"));
	if (!Params->TryGetStringField(TEXT("socket_name"), SocketName))
		return MakeError(TEXT("socket_name required"));

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
		return MakeError(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	const FName SocketFName(*SocketName);
	USkeletalMeshSocket* TargetSocket = nullptr;
	for (USkeletalMeshSocket* Existing : Skeleton->Sockets)
	{
		if (Existing && Existing->SocketName == SocketFName)
		{
			TargetSocket = Existing;
			break;
		}
	}

	if (!TargetSocket)
		return MakeError(FString::Printf(TEXT("Socket not found: %s"), *SocketName));

	Skeleton->Modify();

	if (Params->HasField(TEXT("x")) || Params->HasField(TEXT("y")) || Params->HasField(TEXT("z")))
	{
		FVector Loc = TargetSocket->RelativeLocation;
		double V;
		if (Params->TryGetNumberField(TEXT("x"), V)) Loc.X = V;
		if (Params->TryGetNumberField(TEXT("y"), V)) Loc.Y = V;
		if (Params->TryGetNumberField(TEXT("z"), V)) Loc.Z = V;
		TargetSocket->RelativeLocation = Loc;
	}

	if (Params->HasField(TEXT("pitch")) || Params->HasField(TEXT("yaw")) || Params->HasField(TEXT("roll")))
	{
		FRotator Rot = TargetSocket->RelativeRotation;
		double V;
		if (Params->TryGetNumberField(TEXT("pitch"), V)) Rot.Pitch = V;
		if (Params->TryGetNumberField(TEXT("yaw"), V)) Rot.Yaw = V;
		if (Params->TryGetNumberField(TEXT("roll"), V)) Rot.Roll = V;
		TargetSocket->RelativeRotation = Rot;
	}

	Skeleton->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(Skeleton->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("socket_name"), SocketName);
	Data->SetStringField(TEXT("location"), TargetSocket->RelativeLocation.ToString());
	Data->SetStringField(TEXT("rotation"), TargetSocket->RelativeRotation.ToString());
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleListSockets(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString SkeletonPath;
	if (!Params->TryGetStringField(TEXT("skeleton_path"), SkeletonPath))
		return MakeError(TEXT("skeleton_path required"));

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
		return MakeError(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	TArray<TSharedPtr<FJsonValue>> SocketArray;
	for (USkeletalMeshSocket* Socket : Skeleton->Sockets)
	{
		if (!Socket) continue;
		TSharedPtr<FJsonObject> SocketObj = MakeShared<FJsonObject>();
		SocketObj->SetStringField(TEXT("socket_name"), Socket->SocketName.ToString());
		SocketObj->SetStringField(TEXT("bone_name"), Socket->BoneName.ToString());
		SocketObj->SetStringField(TEXT("location"), Socket->RelativeLocation.ToString());
		SocketObj->SetStringField(TEXT("rotation"), Socket->RelativeRotation.ToString());
		SocketArray.Add(MakeShared<FJsonValueObject>(SocketObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("sockets"), SocketArray);
	Data->SetNumberField(TEXT("count"), SocketArray.Num());
	return MakeResponse(true, Data);
}

// UStaticMesh sockets - a completely separate system from USkeleton sockets
// above (mesh-level, not shared across every skeleton user), for props that
// never touch a skeleton at all. UStaticMesh::Sockets is a plain public
// UPROPERTY with a real ENGINE_API AddSocket() method, unlike USkeleton's
// (see the header comment above) - no IEditableSkeleton-style indirection
// needed, this can construct and add a UStaticMeshSocket directly.
FString FMCPServer::HandleAddStaticMeshSocket(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString MeshPath, SocketName;
	if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath))
		return MakeError(TEXT("mesh_path required"));
	if (!Params->TryGetStringField(TEXT("socket_name"), SocketName))
		return MakeError(TEXT("socket_name required"));

	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh)
		return MakeError(FString::Printf(TEXT("StaticMesh not found: %s"), *MeshPath));

	const FName SocketFName(*SocketName);
	if (Mesh->FindSocket(SocketFName))
		return MakeError(FString::Printf(TEXT("Socket already exists: %s (delete it first via the editor, or pick a new name)"), *SocketName));

	double X = 0, Y = 0, Z = 0, Pitch = 0, Yaw = 0, Roll = 0;
	Params->TryGetNumberField(TEXT("x"), X);
	Params->TryGetNumberField(TEXT("y"), Y);
	Params->TryGetNumberField(TEXT("z"), Z);
	Params->TryGetNumberField(TEXT("pitch"), Pitch);
	Params->TryGetNumberField(TEXT("yaw"), Yaw);
	Params->TryGetNumberField(TEXT("roll"), Roll);

	UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(Mesh);
	Socket->SocketName = SocketFName;
	Socket->RelativeLocation = FVector(X, Y, Z);
	Socket->RelativeRotation = FRotator(Pitch, Yaw, Roll);
	Socket->RelativeScale = FVector(1.0f, 1.0f, 1.0f);

	Mesh->Modify();
	Mesh->AddSocket(Socket);
	UEditorAssetLibrary::SaveAsset(MeshPath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("mesh_path"), MeshPath);
	Data->SetStringField(TEXT("socket_name"), SocketName);
	Data->SetStringField(TEXT("location"), Socket->RelativeLocation.ToString());
	Data->SetStringField(TEXT("rotation"), Socket->RelativeRotation.ToString());
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleListStaticMeshSockets(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString MeshPath;
	if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath))
		return MakeError(TEXT("mesh_path required"));

	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh)
		return MakeError(FString::Printf(TEXT("StaticMesh not found: %s"), *MeshPath));

	TArray<TSharedPtr<FJsonValue>> SocketArray;
	for (UStaticMeshSocket* Socket : Mesh->Sockets)
	{
		if (!Socket) continue;
		TSharedPtr<FJsonObject> SocketObj = MakeShared<FJsonObject>();
		SocketObj->SetStringField(TEXT("socket_name"), Socket->SocketName.ToString());
		SocketObj->SetStringField(TEXT("location"), Socket->RelativeLocation.ToString());
		SocketObj->SetStringField(TEXT("rotation"), Socket->RelativeRotation.ToString());
		SocketObj->SetStringField(TEXT("scale"), Socket->RelativeScale.ToString());
		SocketArray.Add(MakeShared<FJsonValueObject>(SocketObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("sockets"), SocketArray);
	Data->SetNumberField(TEXT("count"), SocketArray.Num());
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetStaticMeshSocketTransform(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString MeshPath, SocketName;
	if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath))
		return MakeError(TEXT("mesh_path required"));
	if (!Params->TryGetStringField(TEXT("socket_name"), SocketName))
		return MakeError(TEXT("socket_name required"));

	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh)
		return MakeError(FString::Printf(TEXT("StaticMesh not found: %s"), *MeshPath));

	const FName SocketFName(*SocketName);
	UStaticMeshSocket* Socket = Mesh->FindSocket(SocketFName);
	if (!Socket)
		return MakeError(FString::Printf(TEXT("Socket not found: %s (use add_static_mesh_socket to create it)"), *SocketName));

	double X = Socket->RelativeLocation.X, Y = Socket->RelativeLocation.Y, Z = Socket->RelativeLocation.Z;
	double Pitch = Socket->RelativeRotation.Pitch, Yaw = Socket->RelativeRotation.Yaw, Roll = Socket->RelativeRotation.Roll;
	Params->TryGetNumberField(TEXT("x"), X);
	Params->TryGetNumberField(TEXT("y"), Y);
	Params->TryGetNumberField(TEXT("z"), Z);
	Params->TryGetNumberField(TEXT("pitch"), Pitch);
	Params->TryGetNumberField(TEXT("yaw"), Yaw);
	Params->TryGetNumberField(TEXT("roll"), Roll);

	Mesh->Modify();
	Socket->RelativeLocation = FVector(X, Y, Z);
	Socket->RelativeRotation = FRotator(Pitch, Yaw, Roll);
	UEditorAssetLibrary::SaveAsset(MeshPath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("mesh_path"), MeshPath);
	Data->SetStringField(TEXT("socket_name"), SocketName);
	Data->SetStringField(TEXT("location"), Socket->RelativeLocation.ToString());
	Data->SetStringField(TEXT("rotation"), Socket->RelativeRotation.ToString());
	return MakeResponse(true, Data);
}
