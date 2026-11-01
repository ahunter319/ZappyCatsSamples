// Copyright Alyssa Hunter 2024.


#include "GalleryImageWidget.h"
#include "Components/Button.h"

void UGalleryImageWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Button_Heart->OnClicked.AddDynamic(this, &UGalleryImageWidget::OnFavoriteClicked);
	Button_Delete->OnClicked.AddDynamic(this, &UGalleryImageWidget::OnDeleteClicked);
}

void UGalleryImageWidget::OnFavoriteClicked()
{
	OnUpdateFavoriteStatus.Broadcast(ScreenshotInfo);

	if (ScreenshotInfo.bFavorited) UnfavoriteButtonStyle();
	else FavoriteButtonStyle();

	ScreenshotInfo.bFavorited = !ScreenshotInfo.bFavorited;
}

void UGalleryImageWidget::OnDeleteClicked()
{
	OnDeleteScreenshotRequested.Broadcast(ScreenshotInfo.FilePath);
}