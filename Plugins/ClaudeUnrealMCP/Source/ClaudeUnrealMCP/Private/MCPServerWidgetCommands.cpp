#include "MCPServer.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/ProgressBar.h"
#include "Components/Spacer.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/GridPanel.h"
#include "Components/PanelWidget.h"
#include "EditorAssetLibrary.h"

// Map widget type string to UClass
static UClass* ResolveWidgetClass(const FString& TypeName)
{
	static const TMap<FString, UClass*> WidgetClasses = {
		{TEXT("CanvasPanel"), UCanvasPanel::StaticClass()},
		{TEXT("VerticalBox"), UVerticalBox::StaticClass()},
		{TEXT("HorizontalBox"), UHorizontalBox::StaticClass()},
		{TEXT("Overlay"), UOverlay::StaticClass()},
		{TEXT("GridPanel"), UGridPanel::StaticClass()},
		{TEXT("ScrollBox"), UScrollBox::StaticClass()},
		{TEXT("SizeBox"), USizeBox::StaticClass()},
		{TEXT("Border"), UBorder::StaticClass()},
		{TEXT("Button"), UButton::StaticClass()},
		{TEXT("TextBlock"), UTextBlock::StaticClass()},
		{TEXT("Image"), UImage::StaticClass()},
		{TEXT("Slider"), USlider::StaticClass()},
		{TEXT("CheckBox"), UCheckBox::StaticClass()},
		{TEXT("EditableTextBox"), UEditableTextBox::StaticClass()},
		{TEXT("ProgressBar"), UProgressBar::StaticClass()},
		{TEXT("Spacer"), USpacer::StaticClass()},
	};

	if (const UClass* const* Found = WidgetClasses.Find(TypeName))
	{
		return const_cast<UClass*>(*Found);
	}

	// Try loading as a custom class path
	UClass* CustomClass = LoadClass<UWidget>(nullptr, *TypeName);
	return CustomClass;
}

FString FMCPServer::HandleCreateWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required (e.g. '/Game/UI')"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
	Factory->ParentClass = UUserWidget::StaticClass();

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, UWidgetBlueprint::StaticClass(), Factory);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create widget blueprint"));

	UWidgetBlueprint* WBP = Cast<UWidgetBlueprint>(NewAsset);

	// Add a default CanvasPanel as root if the widget tree is empty
	if (WBP && WBP->WidgetTree)
	{
		if (!WBP->WidgetTree->RootWidget)
		{
			UCanvasPanel* Root = WBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
			WBP->WidgetTree->RootWidget = Root;
		}
	}

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("name"), AssetName);
	Data->SetBoolField(TEXT("has_root"), WBP && WBP->WidgetTree && WBP->WidgetTree->RootWidget != nullptr);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddWidget(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString BlueprintPath, WidgetType, WidgetName;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
		return MakeError(TEXT("blueprint_path required"));
	if (!Params->TryGetStringField(TEXT("widget_type"), WidgetType))
		return MakeError(TEXT("widget_type required (e.g. TextBlock, Button, Image, VerticalBox, etc.)"));

	Params->TryGetStringField(TEXT("widget_name"), WidgetName);
	if (WidgetName.IsEmpty()) WidgetName = WidgetType + TEXT("_0");

	FString ParentName;
	Params->TryGetStringField(TEXT("parent_name"), ParentName);

	// Load widget blueprint
	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *BlueprintPath);
	if (!WBP)
		return MakeError(FString::Printf(TEXT("Widget blueprint not found: %s"), *BlueprintPath));

	UWidgetTree* WidgetTree = WBP->WidgetTree;
	if (!WidgetTree)
		return MakeError(TEXT("Widget tree is null"));

	// Resolve widget class
	UClass* WidgetClass = ResolveWidgetClass(WidgetType);
	if (!WidgetClass)
		return MakeError(FString::Printf(TEXT("Unknown widget type: %s"), *WidgetType));

	// Create the widget
	UWidget* NewWidget = WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
	if (!NewWidget)
		return MakeError(TEXT("Failed to construct widget"));

	// Find parent (or use root)
	UPanelWidget* Parent = nullptr;
	if (!ParentName.IsEmpty())
	{
		UWidget* ParentWidget = WidgetTree->FindWidget(FName(*ParentName));
		Parent = Cast<UPanelWidget>(ParentWidget);
		if (!Parent)
			return MakeError(FString::Printf(TEXT("Parent '%s' not found or is not a panel widget"), *ParentName));
	}
	else
	{
		Parent = Cast<UPanelWidget>(WidgetTree->RootWidget);
	}

	if (!Parent)
		return MakeError(TEXT("No parent panel available. Create a root panel first."));

	UPanelSlot* Slot = Parent->AddChild(NewWidget);

	// Widgets created through the Designer go through
	// FWidgetBlueprintEditorUtils::CreateWidget, which registers a GUID for
	// the new widget in WidgetVariableNameToGuidMap. ConstructWidget above
	// bypasses that entirely, so without this the compiler's own internal
	// consistency check (WidgetBlueprintCompiler.cpp, "Widget was added but
	// did not get a GUID") fails - as an ensure() on some widget types, but
	// as a hard crash immediately after on others (hit via a Button widget;
	// see AGENTS.md). Harmless to add for every widget even if it never
	// becomes a Blueprint variable.
	if (!WBP->WidgetVariableNameToGuidMap.Contains(NewWidget->GetFName()))
	{
		WBP->WidgetVariableNameToGuidMap.Add(NewWidget->GetFName(), FGuid::NewGuid());
	}

	// Set canvas slot position if parent is CanvasPanel
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		double PosX = 0, PosY = 0, SizeX = 200, SizeY = 50;
		Params->TryGetNumberField(TEXT("x"), PosX);
		Params->TryGetNumberField(TEXT("y"), PosY);
		Params->TryGetNumberField(TEXT("width"), SizeX);
		Params->TryGetNumberField(TEXT("height"), SizeY);
		CanvasSlot->SetPosition(FVector2D(PosX, PosY));
		CanvasSlot->SetSize(FVector2D(SizeX, SizeY));
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), NewWidget->GetName());
	Data->SetStringField(TEXT("type"), WidgetType);
	Data->SetStringField(TEXT("parent"), Parent->GetName());
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleRemoveWidget(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString BlueprintPath, WidgetName;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
		return MakeError(TEXT("blueprint_path required"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return MakeError(TEXT("widget_name required"));

	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *BlueprintPath);
	if (!WBP || !WBP->WidgetTree)
		return MakeError(TEXT("Widget blueprint not found or has no widget tree"));

	UWidget* Widget = WBP->WidgetTree->FindWidget(FName(*WidgetName));
	if (!Widget)
		return MakeError(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));

	if (Widget == WBP->WidgetTree->RootWidget)
		return MakeError(TEXT("Cannot remove the root widget"));

	const bool bRemoved = WBP->WidgetTree->RemoveWidget(Widget);
	if (!bRemoved)
		return MakeError(FString::Printf(TEXT("Failed to remove widget: %s"), *WidgetName));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Removed widget %s (and its children) from %s"), *WidgetName, *WBP->GetName()));
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString BlueprintPath, WidgetName, PropertyName, Value;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
		return MakeError(TEXT("blueprint_path required"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return MakeError(TEXT("widget_name required"));
	if (!Params->TryGetStringField(TEXT("property"), PropertyName))
		return MakeError(TEXT("property required"));
	if (!Params->TryGetStringField(TEXT("value"), Value))
		return MakeError(TEXT("value required"));

	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *BlueprintPath);
	if (!WBP || !WBP->WidgetTree)
		return MakeError(TEXT("Widget blueprint not found or has no widget tree"));

	UWidget* Widget = WBP->WidgetTree->FindWidget(FName(*WidgetName));
	if (!Widget)
		return MakeError(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));

	// Try setting common properties directly
	if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
	{
		if (PropertyName == TEXT("Text"))
		{
			TextBlock->SetText(FText::FromString(Value));
			goto Done;
		}
	}

	// Generic property setter via FProperty
	{
		FProperty* Prop = FindFProperty<FProperty>(Widget->GetClass(), *PropertyName);
		UObject* PropOwner = Widget;

		// Layout properties (Size, Padding, HorizontalAlignment,
		// VerticalAlignment, ...) live on the child's UPanelSlot (e.g.
		// UHorizontalBoxSlot::Size, a FSlateChildSize{SizeRule,Value} - used
		// to give a Slider real width inside a HorizontalBox row instead of
		// the tiny default auto-sized one), not on the UWidget itself. Fall
		// back to the slot's class when the widget class doesn't have it.
		if (!Prop && Widget->Slot)
		{
			Prop = FindFProperty<FProperty>(Widget->Slot->GetClass(), *PropertyName);
			PropOwner = Widget->Slot;
		}

		if (!Prop)
			return MakeError(FString::Printf(TEXT("Property '%s' not found on widget class %s or its slot"), *PropertyName, *Widget->GetClass()->GetName()));

		void* ValPtr = Prop->ContainerPtrToValuePtr<void>(PropOwner);
		const TCHAR* Result = Prop->ImportText_Direct(*Value, ValPtr, PropOwner, PPF_None);
		if (!Result)
			return MakeError(FString::Printf(TEXT("Failed to import value '%s' for property %s"), *Value, *PropertyName));
	}

Done:
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("widget"), WidgetName);
	Data->SetStringField(TEXT("property"), PropertyName);
	Data->SetBoolField(TEXT("set"), true);
	return MakeResponse(true, Data);
}

static TSharedPtr<FJsonObject> SerializeWidgetNode(UWidget* Widget, UWidgetTree* Tree)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	if (!Widget) return Obj;

	Obj->SetStringField(TEXT("name"), Widget->GetName());
	Obj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
	Obj->SetBoolField(TEXT("is_visible"), Widget->GetVisibility() == ESlateVisibility::Visible ||
		Widget->GetVisibility() == ESlateVisibility::HitTestInvisible ||
		Widget->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);

	// If it's a panel, list children
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		TArray<TSharedPtr<FJsonValue>> Children;
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			UWidget* Child = Panel->GetChildAt(i);
			if (Child)
			{
				Children.Add(MakeShared<FJsonValueObject>(SerializeWidgetNode(Child, Tree)));
			}
		}
		Obj->SetArrayField(TEXT("children"), Children);
	}

	// Common properties
	if (UTextBlock* TB = Cast<UTextBlock>(Widget))
	{
		Obj->SetStringField(TEXT("text"), TB->GetText().ToString());
	}

	return Obj;
}

FString FMCPServer::HandleReadWidgetTree(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
		return MakeError(TEXT("blueprint_path required"));

	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *BlueprintPath);
	if (!WBP || !WBP->WidgetTree)
		return MakeError(TEXT("Widget blueprint not found or has no widget tree"));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint"), BlueprintPath);

	if (WBP->WidgetTree->RootWidget)
	{
		Data->SetObjectField(TEXT("root"), SerializeWidgetNode(WBP->WidgetTree->RootWidget, WBP->WidgetTree));
	}

	// Total widget count
	TArray<UWidget*> AllWidgets;
	WBP->WidgetTree->GetAllWidgets(AllWidgets);
	Data->SetNumberField(TEXT("total_widgets"), AllWidgets.Num());

	return MakeResponse(true, Data);
}
